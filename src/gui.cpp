#include "gui.hpp"
#include "store_controller.hpp"
#include <QApplication>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <iostream>
#include <qboxlayout.h>
#include <string>

MainWindow::MainWindow(StoreController &storeController, QWidget *parent)
    : QMainWindow(parent), storeController(storeController) {

  // Create layout
  setupUi();

  // Connect functionality
  setupConnections();
}

void MainWindow::onSetKey() {
  std::string user_input_key = set_key_input->text().toStdString();
  std::string user_input_value = set_value_input->text().toStdString();

  SetResult result =
      storeController.handleSetKeyValue(user_input_key, user_input_value);

  if (!result.success) {
    set_result->setText(QString::fromStdString(result.error));
  } else {
    set_result->setText("Success");
    set_timer_results->setText(
        QString::fromStdString("Operation completed in " +
                               std::to_string(result.time_elapsed)) +
        "ms");
    long curr_writes = std::stoi(total_writes->text().toStdString());
    total_writes->setText(
        QString::fromStdString(std::to_string(++curr_writes)));
  }

  set_key_input->clear();
  set_value_input->clear();
}

void MainWindow::onGetKey() {
  std::string user_input_key = get_key_input->text().toStdString();

  GetResult result = storeController.handleGetValue(user_input_key);
  if (result.success) {
    get_result->setText(QString::fromStdString(result.value));
    get_timer_results->setText(
        QString::fromStdString("Operation completed in " +
                               std::to_string(result.time_elapsed)) +
        "ms");
  } else {
    get_result->setText(QString::fromStdString(result.error));
  }

  get_key_input->clear();
}

void MainWindow::onDeleteKey() {
  std::string user_input_key = set_key_input->text().toStdString();

  DeleteResult result = storeController.handleDeleteKey(user_input_key);
  if (result.success) {
    set_result->setText("Deleted successfully");
    get_timer_results->setText(QString::fromStdString(
        "Operation completed in " + std::to_string(result.time_elapsed)));
  } else {
    set_result->setText("Deletion failed");
    std::cout << result.error << std::endl;
  }
  set_key_input->clear();
}

void MainWindow::onSeedEntries() {
  BulkSetResult result = storeController.handleSeedEntries(1000);
  long curr_writes = std::stoi(total_writes->text().toStdString());
  total_writes->setText(QString::fromStdString(
      std::to_string(curr_writes + result.writes_completed)));
}

void MainWindow::setupUi() {

  QWidget *central = new QWidget(this);
  setCentralWidget(central);

  /* Left side widgets (set new key/value) */
  QWidget *left_parent = new QWidget(central);
  QGroupBox *stats_box_left =
      new QGroupBox("Operation statistics", left_parent);
  set_key_input = new QLineEdit(left_parent);
  set_value_input = new QLineEdit(left_parent);
  set_submit_button = new QPushButton("Set new key/value", left_parent);
  delete_submit_button = new QPushButton("Delete key", left_parent);
  seed_entries_button = new QPushButton("Seed entries", left_parent);

  QLabel *set_result_title =
      new QLabel("Result of last operation", stats_box_left);
  set_result = new QLabel("N/A", stats_box_left);

  QLabel *total_writes_title =
      new QLabel("Total completed writes", stats_box_left);
  total_writes = new QLabel("0", stats_box_left);

  set_timer_results = new QLabel(stats_box_left);

  /* Left side layout */
  QHBoxLayout *left_parent_layout = new QHBoxLayout(left_parent);
  QVBoxLayout *left_operation_layout = new QVBoxLayout();
  QHBoxLayout *left_operation_buttons = new QHBoxLayout();
  QHBoxLayout *action_buttons_layout = new QHBoxLayout();
  QVBoxLayout *left_statistic_layout = new QVBoxLayout(stats_box_left);
  QHBoxLayout *left_result_layout = new QHBoxLayout();
  QVBoxLayout *total_writes_layout = new QVBoxLayout();

  left_parent_layout->addLayout(left_operation_layout);
  left_parent_layout->addWidget(stats_box_left);

  left_operation_buttons->addWidget(set_submit_button);
  left_operation_buttons->addWidget(delete_submit_button);

  left_operation_layout->addWidget(set_key_input);
  left_operation_layout->addWidget(set_value_input);
  action_buttons_layout->addLayout(left_operation_buttons);
  action_buttons_layout->addWidget(seed_entries_button);
  left_operation_layout->addLayout(action_buttons_layout);

  left_result_layout->addWidget(set_result_title);
  left_result_layout->addWidget(set_result);

  total_writes_layout->addWidget(total_writes_title);
  total_writes_layout->addWidget(total_writes);
  total_writes_layout->setSpacing(6);

  left_statistic_layout->setSpacing(12);
  left_statistic_layout->addLayout(left_result_layout);
  left_statistic_layout->addWidget(set_timer_results);
  left_statistic_layout->addLayout(total_writes_layout);

  /* Right side widgets (get value from key) */
  QWidget *right_parent = new QWidget(central);
  QGroupBox *stats_box_right =
      new QGroupBox("Get Operation Statistics", right_parent);
  get_key_input = new QLineEdit(right_parent);
  get_submit_button = new QPushButton("Submit", right_parent);
  QLabel *get_result_title =
      new QLabel("Result of last operation", stats_box_right);
  get_result = new QLabel(stats_box_right);
  get_timer_results = new QLabel(stats_box_right);

  /* Right side layout */
  QHBoxLayout *right_parent_layout = new QHBoxLayout(right_parent);
  QVBoxLayout *right_operation_layout = new QVBoxLayout();
  QHBoxLayout *right_operation_buttons = new QHBoxLayout();
  QVBoxLayout *right_statistic_layout = new QVBoxLayout(stats_box_right);
  QHBoxLayout *right_result_layout = new QHBoxLayout();

  right_parent_layout->addWidget(get_key_input);
  right_parent_layout->addWidget(get_submit_button);
  right_parent_layout->addLayout(right_operation_layout);
  right_parent_layout->addWidget(stats_box_right);

  right_operation_layout->addWidget(get_result);
  right_operation_layout->addWidget(get_timer_results);

  right_result_layout->addWidget(get_result_title);
  right_result_layout->addWidget(get_result);

  right_statistic_layout->setSpacing(12);
  right_statistic_layout->addLayout(right_result_layout);
  right_statistic_layout->addWidget(get_timer_results);

  /* Main layout */
  QHBoxLayout *main_layout = new QHBoxLayout(central);
  main_layout->addWidget(left_parent);
  main_layout->addWidget(right_parent);
}

void MainWindow::setupConnections() {

  connect(set_submit_button, &QPushButton::clicked, this,
          &MainWindow::onSetKey);

  connect(get_submit_button, &QPushButton::clicked, this,
          &MainWindow::onGetKey);

  connect(seed_entries_button, &QPushButton::clicked, this,
          &MainWindow::onSeedEntries);

  connect(delete_submit_button, &QPushButton::clicked, this,
          &MainWindow::onDeleteKey);
}
