/*
 * This software is Licensed under the Apache License Version 2.0
 * See LICENSE
 *
 * ToottContent クラス
 * タイムラインのトゥート一つ一つの(HTMLで言う)divみたいなもの
 */
#include "../Mastodon/MastodonUrl.h"
#include "../Mastodon/TootData.h"
#include "../Sweetfish.h"
#include "ImageLabel.h"
#include "ImageViewer.h"
#include "TextLabel.h"
#include "TootContent.h"
#include "VideoPlayer.h"
#include <QNetworkReply>
#include <QtWidgets>

// Network TootContent::net;
// staticだと終了時スレッドが残ってると怒られる(一番最後まで残るから?)

TootContent::TootContent(TootData *init_tdata, Mode init_mode,
                         QWidget *_root_widget /*ImageViewerなどに渡す*/,
                         QWidget *parent, Qt::WindowFlags f)
    : QFrame(parent, f), mode(init_mode), root_widget(_root_widget),
      popup(nullptr) {
  setTootData(init_tdata);
  drawToot();
}

TootContent::~TootContent() { delete tdata; }

/*
 * 引数:tdata(新TootData), should_redraw(再描画すべきか)
 * 戻値:なし
 * 概要:tdataを更新するときに使う。元のtdataの削除はしないので呼び出し元でする。
 */
void TootContent::setTootData(TootData *target_tdata, bool should_redraw) {
  tdata = target_tdata;
  if (should_redraw)
    redrawToot();
}

/*
 * 引数:なし
 * 戻値:tdata(所持してるTootData)
 * 概要:管理しているtdataを返す。
 */
TootData *TootContent::getTootData() { return tdata; }

/*
 * 引数:event
 * 戻値:なし
 * 概要:マウス操作がされたときに呼び出される。
 */
void TootContent::mousePressEvent(QMouseEvent *event) {
  switch (event->button()) {
  case Qt::RightButton:
    if (!popup) {
      popup = new QMenu(tdata->getAccountData().getDisplayName(),
                        this); // Title表示されない...
      createActions();
    }
    popup->popup(event->globalPos());
    break;
  default:
    event->ignore();
    return;
  }
  event->accept();
}

/*
 * 引数:なし
 * 戻値:なし
 * 概要:ポップアップメニュー作成。
 */
void TootContent::createActions() {
  if (mode & Mode::Info) {
    popup
        ->addAction(
            this->style()->standardIcon(
                QStyle::SP_TitleBarCloseButton) /*少し意図がずれてる気が*/,
            tr("Delete(&D)"), this, &TootContent::triggeredAction)
        ->setData('d');
    return; //最低限の表示のみ
  }
  //リトゥート
  popup->addSection((tdata->getContent().size() > 15)
                        ? tdata->getContent().left(15).append("...")
                        : tdata->getContent()); //使い方合ってるんかいな...
  popup
      ->addAction(QIcon(":/bst.png"), tr("Boost(&B)"), this,
                  &TootContent::triggeredAction)
      ->setData('b');

  //引用トゥート
  popup
      ->addAction(QIcon(":/bst.png"), tr("Quote Toot(&Q)"), this,
                  &TootContent::triggeredAction)
      ->setData('q');

  //リプライ
  popup
      ->addAction(QIcon(":/rp.png"), tr("Reply(&R)"), this,
                  &TootContent::triggeredAction)
      ->setData('r');

  //お気に入り(今はいいねだが...)
  popup
      ->addAction(QIcon(":/fav.png"), tr("Favourite(&F)"), this,
                  &TootContent::triggeredAction)
      ->setData('f');

  //削除(権限がある場合)
  if (tdata->isTootOwner())
    popup
        ->addAction(
            style()->standardIcon(
                QStyle::SP_TitleBarCloseButton) /*少し意図がずれてる気が*/,
            tr("Delete(&D)"), this, &TootContent::triggeredAction)
        ->setData('d');
  //新しいウィンドウで開く(取っておきたいなど)
  popup->addAction(style()->standardIcon(QStyle::SP_TitleBarMaxButton),
                   tr("Open in new window(&W)"), this,
                   &TootContent::openWindow);

  if (const TootUrlData url_list = tdata->getUrlData(); url_list.count() != 0) {
    popup->addSection(tr("URL"));
    for (unsigned int cnt = 0, size = url_list.count(); cnt < size; cnt++) {
      QAction *url_open_action =
          new QAction((url_list.getDisplayUrl(cnt).size() > 15)
                          ? url_list.getDisplayUrl(cnt).left(15).append("...")
                          : url_list.getDisplayUrl(cnt),
                      popup);
      url_open_action->setData(cnt);
      connect(url_open_action, &QAction::triggered, this,
              &TootContent::openUrl);
      popup->addAction(url_open_action);
    }
  }

  if (!tdata->getApplicationName().isEmpty()) {
    popup->addSection(tr("クライアント"));
    popup
        ->addAction(
            tdata->getApplicationName(), this,
            [this] {
              QDesktopServices::openUrl(QUrl(
                  ((qobject_cast<QAction *>(sender()))->data().toString())));
            })
        ->setData(tdata->getApplicationSite());
  }
}

/*
 * 引数:なし
 * 戻値:なし
 * 概要:各種アクションが押されたときに呼ばれる。tdataとActionとともにactionシグナルを呼ぶだけ。
 * 備考:各種設定をしてQtでenumを認識できるようにしてもdata().vaiue<TwitterCore::Action>で取り出せないので諦めてuintでわたす。
 */
void TootContent::triggeredAction() {
  return emit action(
      tdata, (qobject_cast<QAction *>(sender()))->data().toChar().toLatin1());
}

/*
 * 引数:なし
 * 戻値:なし
 * 概要:クリックされたURLをブラウザで開く。
 */
void TootContent::openUrl() {
  QString &&url = tdata->getUrlData().getFullUrl(
      qobject_cast<QAction *>(sender())->data().toInt());
  QDesktopServices::openUrl(url);
}

/*
 * 引数:なし
 * 戻値:なし
 * 概要:もう一度TootDataを使ってトゥートを表示し直す
 */
void TootContent::redrawToot() {
  delete layout();
  drawToot();
}

/*
 * 引数:なし
 * 戻値:なし
 * 概要:TootDataを使ってトゥートを表示する
 */
void TootContent::drawToot() {
  if (tdata == nullptr)
    return;
  QHBoxLayout *main_box = new QHBoxLayout;
  QVBoxLayout *text_box = new QVBoxLayout;

  text_box->setSpacing(0);
  ImageLabel *icon = new ImageLabel(40, 40);
  icon->setFixedSize(40, 40);
  if (!icon->setPixmapByName(tdata->getOriginalAccountData()
                                 .getAvatar())) { //アイコンのキャッシュがない
    connect(net.get(tdata->getOriginalAccountData().getAvatar()),
            &QNetworkReply::finished, icon, &ImageLabel::setPixmapByNetwork);
  }
  main_box->addWidget(icon, 0, Qt::AlignTop);

  if (tdata->getBoostedData() != nullptr) { // Boost
    QLabel *boosted_user_name =
        new QLabel(tdata->getAccountData().getDisplayName() + tr(" boosted"));
    boosted_user_name->setStyleSheet(
        "font-size:10px;color:lime;"); // small指定ができない
    boosted_user_name->setWordWrap(true);
    text_box->addWidget(boosted_user_name);
  }

  QLabel *display_name =
      new QLabel(tdata->getOriginalAccountData().getDisplayName());
  display_name->setStyleSheet("font-weight:bold;color:white;");
  display_name->setWordWrap(true);
  text_box->addWidget(display_name);
  QLabel *user_name =
      new QLabel('@' + tdata->getOriginalAccountData().getAcct());
  user_name->setStyleSheet("font-size:10px;color:gray;");
  user_name->setWordWrap(true);
  text_box->addWidget(user_name);

  if (tdata->getOriginalAccountData().isLocked()) {
    user_name->setText(user_name->text() + "<img src=\":/protected.png\" />");
    user_name->setTextFormat(Qt::RichText);
  }

  text_box->addWidget(new TextLabel(tdata->getContent()));
  if (tdata->getMediaData().size() && !(mode & Mode::Info)) {
    TootMediaData media_data = tdata->getMediaData();
    if (media_data.getEntry(0).getType() == "video" ||
        media_data.getEntry(0).getType() == "gifv") { //動画(一つのみ対応)
      TootMediaDataEntry video_entry = media_data.getEntry(0);

      ImageLabel *iml = new ImageLabel(50, 50, 0, this);
      if (!iml->setPixmapByName(video_entry.getPreviewUrl())) { //キャッシュなし
        // if ( tdata->flag & 0x40 ) iml->setPixmap ( style()->standardIcon (
        // QStyle::SP_MessageBoxWarning ).pixmap ( 100, 45 ) );
        /*else*/ connect(net.get(video_entry.getPreviewUrl()),
                         &QNetworkReply::finished, iml,
                         &ImageLabel::setPixmapByNetwork);
      }
      connect(iml, &ImageLabel::clicked, this, &TootContent::showPicture);
      iml->setStyleSheet("border:3px solid blue;"); //動画かどうかの判別(Beta)
      text_box->addWidget(iml);
    } else { //画像
      QScrollArea *media_box = new QScrollArea;
      media_box->setWidgetResizable(true);
      QWidget *center = new QWidget;
      QHBoxLayout *media_layout = new QHBoxLayout(center);
      media_box->setWidget(center);

      for (unsigned int cnt = 0, size = media_data.size(); cnt < size; cnt++) {
        TootMediaDataEntry image_entry = media_data.getEntry(cnt);
        if (image_entry.getType() != "image") {
          continue;
        }
        ImageLabel *iml = new ImageLabel(
            100, 45 /*50にするとMediaBoxにY軸スクロールバーがついて気持ち悪い*/,
            cnt, this);

        if (!iml->setPixmapByName(
                image_entry.getPreviewUrl())) { //キャッシュなし
          /* iml->setPixmap(style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(100,
           * 45));*/
          connect(net.get(image_entry.getPreviewUrl()),
                  &QNetworkReply::finished, iml,
                  &ImageLabel::setPixmapByNetwork);
        }
        connect(iml, &ImageLabel::clicked, this, &TootContent::showPicture);
        media_layout->addWidget(iml);
      }
      text_box->addWidget(media_box);
    }
  }
  //引用(独自機能)
  // BETTER: Mastodon本体の改造を促すべき
  for (unsigned int cnt = 0, size = tdata->getUrlData().size(); cnt < size;
       cnt++) {
    QStringList url =
        tdata->getUrlData().getFullUrl(cnt).split('/', QString::SkipEmptyParts);
    QString id;
    int url_size = url.size(); // URLを'/'で区切った時の要素数
    if (url_size < 4)
      continue;
    if (url_size >= 6 && url[2] == "users" && url[4] == "statuses") {
      id = url[5];
    } else if (url_size >= 5 && url[2] == "web" && url[3] == "statuses") {
      id = url[4];
    } else if (url[2].at(0) == '@') {
      id = url[3];
    }
    if (!QRegExp("^\\d+$").exactMatch(id))
      continue;

    TootContent *quote = new TootContent;
    quote->setFrameShape(QFrame::StyledPanel);
    quote->setFrameShadow(QFrame::Sunken);
    text_box->addWidget(quote);
    connect(
        net.get(MastodonUrl::scheme + url[1] + MastodonUrl::statuses + "/" +
                id),
        &QNetworkReply::finished, this, [this, quote] {
          QNetworkReply *rep = qobject_cast<QNetworkReply *>(sender());
          quote->setTootData(
              new TootData(QJsonDocument::fromJson(rep->readAll()).object()),
              true);
          rep->deleteLater();
        });
  }
  //その他情報
  QLabel *info_text = new QLabel(tdata->getDateTime()
                                     .toTimeSpec(Qt::LocalTime)
                                     .toString(Qt::SystemLocaleShortDate));
  info_text->setWordWrap(true);
  info_text->setStyleSheet("font-size:10px;color:gray;");
  text_box->addWidget(info_text, 0, Qt::AlignRight);
  main_box->addLayout(text_box);
  setLayout(main_box);
}

/*
 * 引数:url(表示する画像のURL)
 * 戻値:なし
 * 概要:大きい画像を表示する。または動画を再生する。
 */
void TootContent::showPicture(TootData *tdata, unsigned int index) {
  if (tdata == nullptr || !tdata->getMediaData().size())
    return;
  // if ( tdata->flag & 0x40 && QMessageBox::question ( root_widget, APP_NAME,
  // tr (
  // "この画像・動画を表示すると気分を害する可能性があります。表示しますか。" ),
  // QMessageBox::Yes | QMessageBox::No, QMessageBox::No ) != QMessageBox::Yes )
  // return;
  if (tdata->getMediaData().getEntry(0).getType() == "video") { //動画
    VideoPlayer *player = new VideoPlayer(tdata, root_widget, Qt::Window);
    player->show();
  } else { //画像
    ImageViewer *viewer =
        new ImageViewer(tdata, index, root_widget, Qt::Window);
    viewer->show();
  }
}

/*
 * 引数:なし
 * 戻値:なし
 * 概要:自分をコピーして新たなウィンドウとして開く
 */
void TootContent::openWindow() {
  if (tdata == nullptr)
    return;
  TootContent *window = new TootContent(new TootData(*tdata), mode, root_widget,
                                        root_widget, Qt::Window);
  QPalette Palette = window->palette();
  window->setAttribute(Qt::WA_DeleteOnClose);
  window->setWindowTitle(tr("トゥートの詳細 ") + APP_NAME);
  Palette.setColor(QPalette::Window, Qt::black); //背景を黒く
  Palette.setColor(QPalette::WindowText, Qt::white);
  window->setAutoFillBackground(true);
  window->setPalette(Palette);
  window->resize(sizeHint());
  window->show();
}

/*
 * 引数:ori(root_widgetに手渡すTootData、act(選択された操作)
 * 戻値:なし
 * 概要:root_widgetにactionシグナルを転送する。もっといい方法ないかな。
 */
void TootContent::transferAction(TootData *ori, unsigned char act) {
  return emit action(ori, act);
}
