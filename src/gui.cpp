#include "gui.hpp"
#include "store_controller.hpp"
#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <string>

MainWindow::MainWindow(StoreController &storeController, QWidget *parent)
    : QMainWindow(parent), storeController(storeController) {

  // Create layout
  setupUi();

  // Connect functionality
  setupConnections();
}

void MainWindow::onSetSubmitKey() {
  std::string user_input_key = set_key_input->text().toStdString();
  std::string user_input_value = set_value_input->text().toStdString();

  SetResult result =
      storeController.handleSetKeyValue(user_input_key, user_input_value);

  if (!result.success) {
    set_result->setText(QString::fromStdString(result.error));
  } else {
    set_result->setText("Success");
    set_timer_results->setText(
        QString::fromStdString(std::to_string(result.time_elapsed)));
  }

  set_key_input->clear();
  set_value_input->clear();
}

void MainWindow::onGetSubmitKey() {
  std::string user_input_key = get_key_input->text().toStdString();

  GetResult result = storeController.handleGetValue(user_input_key);
  if (result.success) {
    get_result->setText(QString::fromStdString(result.value));
    get_timer_results->setText(
        QString::fromStdString(std::to_string(result.time_elapsed)));
  } else {
    get_result->setText(QString::fromStdString(result.error));
  }

  get_key_input->clear();
}

void MainWindow::setupUi() {

  QWidget *central = new QWidget(this);
  setCentralWidget(central);

  /* Left side widgets (set new key/value) */
  QWidget *set_widget_left = new QWidget(central);
  set_key_input = new QLineEdit(set_widget_left);
  set_value_input = new QLineEdit(set_widget_left);
  set_submit_button = new QPushButton("Submit", set_widget_left);
  set_result = new QLabel(set_widget_left);
  set_timer_results = new QLabel(set_widget_left);

  /* Left side layout */
  QVBoxLayout *left_layout = new QVBoxLayout(set_widget_left);
  left_layout->addWidget(set_key_input);
  left_layout->addWidget(set_value_input);
  left_layout->addWidget(set_submit_button);
  left_layout->addWidget(set_result);
  left_layout->addWidget(set_timer_results);

  /* Right side widgets (get value from key) */
  QWidget *get_widget_right = new QWidget(central);
  get_key_input = new QLineEdit(get_widget_right);
  get_submit_button = new QPushButton("Submit", get_widget_right);
  get_result = new QLabel(get_widget_right);
  get_timer_results = new QLabel(get_widget_right);

  /* Right side layout */
  QVBoxLayout *right_layout = new QVBoxLayout(get_widget_right);
  right_layout->addWidget(get_key_input);
  right_layout->addWidget(get_submit_button);
  right_layout->addWidget(get_result);
  right_layout->addWidget(get_timer_results);

  /* Main layout */
  QHBoxLayout *main_layout = new QHBoxLayout(central);
  main_layout->addWidget(set_widget_left);
  main_layout->addWidget(get_widget_right);
}

void MainWindow::setupConnections() {

  connect(set_submit_button, &QPushButton::clicked, this,
          &MainWindow::onSetSubmitKey);

  connect(get_submit_button, &QPushButton::clicked, this,
          &MainWindow::onGetSubmitKey);
}
