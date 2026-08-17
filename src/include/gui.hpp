#pragma once
#include "store_controller.hpp"
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(StoreController &storeController,
                      QWidget *parent = nullptr);
  ~MainWindow() = default;

private:
  StoreController storeController;
  QLineEdit *set_key_input;
  QLineEdit *set_value_input;
  QPushButton *set_submit_button;
  QLineEdit *get_key_input;
  QPushButton *get_submit_button;
  QLabel *get_result;
  QLabel *set_result;
  QLabel *set_timer_results;
  QLabel *get_timer_results;
  void onSetSubmitKey();
  void onGetSubmitKey();
  void setupUi();
  void setupConnections();
};
