#pragma once
#include "store.hpp"
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(KV_Store &store, QWidget *parent = nullptr);
  ~MainWindow() = default;

private:
  QLineEdit *set_key_input;
  QLineEdit *set_value_input;
  QPushButton *set_submit_button;
  QLineEdit *get_key_input;
  QPushButton *get_submit_button;
  QLabel *get_result;
  KV_Store store;
  void onSetSubmitKey();
  void onGetSubmitKey();
};
