/*
 * This software is Licensed under the Apache License Version 2.0
 * See LICENSE
 *
 * TootContent クラス
 * タイムラインのトゥート一つ一つの(HTMLで言う)divみたいなもの
 */
#pragma once

#include "../Network/Network.h"
#include <QFrame>

class QMouseEvent;
class QMenu;
class TootData;

class TootContent : public QFrame {
  Q_OBJECT
public:
  enum Mode { //主にメニューの表示内容の制御
    Normal,
    Reply = 1 << 1,
    Info = 1 << 2,
    Simple = 1 << 3,
  };

  explicit TootContent(TootData *init_tdata = nullptr,
                       Mode init_mode = Mode::Normal,
                       QWidget *init_root_widget = Q_NULLPTR,
                       QWidget *parent = Q_NULLPTR,
                       Qt::WindowFlags f = Qt::WindowFlags());
  virtual ~TootContent();
  void setTootData(TootData *target_tdata, bool should_redraw = false);
  TootData *getTootData();

signals:
  void action(TootData *ori, unsigned char act);

private slots:
  void triggeredAction();
  void openUrl();
  void showPicture(TootData *tdata, unsigned int index);
  void openWindow();
  void transferAction(TootData *ori, unsigned char act);

private:
  virtual void mousePressEvent(QMouseEvent *event) override;
  void drawToot();
  void redrawToot();
  void createActions();
  Mode mode;
  Network net;
  QWidget *root_widget;
  QMenu *popup;
  TootData *tdata;
};
