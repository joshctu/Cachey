#include "gui.hpp"
#include "exceptions.hpp"
#include "store.hpp"
#include <QApplication>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>
#include <cctype>
#include <iostream>
#include <qboxlayout.h>
#include <qlineedit.h>
#include <qtoolbar.h>
#include <qwidget.h>

MainWindow::MainWindow(KV_Store &store, QWidget *parent)
    : store(store), QMainWindow(parent) {

  // Create layout
  QWidget *central = new QWidget(this);
  QHBoxLayout *main_layout = new QHBoxLayout(central);

  QWidget *set_widget_left = new QWidget(central);
  QWidget *get_widget_right = new QWidget(central);

  QVBoxLayout *left_layout = new QVBoxLayout(set_widget_left);
  QVBoxLayout *right_layout = new QVBoxLayout(get_widget_right);

  main_layout->addWidget(set_widget_left);
  main_layout->addWidget(get_widget_right);

  // Create setter key/value components and layout
  set_key_input = new QLineEdit(set_widget_left);
  set_value_input = new QLineEdit(set_widget_left);
  set_submit_button = new QPushButton("Submit", set_widget_left);

  left_layout->addWidget(set_key_input);
  left_layout->addWidget(set_value_input);
  left_layout->addWidget(set_submit_button);

  // Create getter key components and layout
  get_key_input = new QLineEdit(get_widget_right);
  get_submit_button = new QPushButton("Submit", get_widget_right);
  get_result = new QLabel(get_widget_right);

  right_layout->addWidget(get_key_input);
  right_layout->addWidget(get_submit_button);
  right_layout->addWidget(get_result);

  setCentralWidget(central);

  connect(this->set_submit_button, &QPushButton::clicked, this,
          &MainWindow::onSetSubmitKey);

  connect(this->get_submit_button, &QPushButton::clicked, this,
          &MainWindow::onGetSubmitKey);
};

void MainWindow::onSetSubmitKey() {
  std::string user_input_key = set_key_input->text().toStdString();
  std::string user_input_value = set_value_input->text().toStdString();

  if (user_input_key.size() == 0 || user_input_value.size() == 0)
    return;

  try {
    store.set(user_input_key, user_input_value);
  } catch (WriteException &e) {
    std::cout << e.what() << std::endl;
  }

  set_key_input->clear();
  set_value_input->clear();
  std::cout << "Successfully logged" << "\n";
};

void MainWindow::onGetSubmitKey() {
  std::string user_input_key = get_key_input->text().toStdString();

  if (user_input_key.size() == 0)
    return;
  try {
    std::optional<std::string> result = store.get(user_input_key);
    if (result) {
      std::string valid_result = *result;
      QString qresult = QString::fromStdString(valid_result);
      get_result->setText(qresult);
    }
  } catch (ReadException &e) {
    std::cout << e.what() << std::endl;
  }
}
