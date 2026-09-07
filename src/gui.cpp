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
#include <qsizepolicy.h>
#include <string>

MainWindow::MainWindow(StoreController &storeController, QWidget *parent)
    : QMainWindow(parent), storeController(storeController) {

  // Create layout
  setupUi();

  // Connect functionality
  setupConnections();
}

/**
 * @brief This handler is called when the set new key button is pressed.
 * Hydrates gui with store return data.
 *
 */
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

/**
 * @brief This handler is called when the get key button is pressed. Hydrates
 * gui with store return data.
 */
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

/**
 * @brief This handler is called when the delete key button is pressed. Hydrates
 * gui with store return data.
 */
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

/**
 * @brief This handler is called when the seed entries button is pressed. Seeds
 * 1000 consecutive entries with the shape key<int> / value<int>
 */
void MainWindow::onSeedEntries() {
  BulkSetResult result = storeController.handleSeedEntries(1000);
  long curr_writes = std::stoi(total_writes->text().toStdString());
  total_writes->setText(QString::fromStdString(
      std::to_string(curr_writes + result.writes_completed)));
}

QWidget *MainWindow::createLeftPanel() {
  QWidget *w = new QWidget;
  w->setMinimumWidth(500);
  w->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);

  auto *opLayout = new QVBoxLayout; // operation inputs + buttons
  set_key_input = new QLineEdit;
  set_value_input = new QLineEdit;
  opLayout->addWidget(set_key_input);
  opLayout->addWidget(set_value_input);

  auto *buttons = new QHBoxLayout;
  set_submit_button = new QPushButton("Set key");
  delete_submit_button = new QPushButton("Delete key");
  seed_entries_button = new QPushButton("Seed entries");
  buttons->addWidget(set_submit_button);
  buttons->addWidget(delete_submit_button);
  buttons->addWidget(seed_entries_button);
  opLayout->addLayout(buttons);

  auto *statsBox = new QGroupBox("Set Operation Statistics");
  auto *statsLayout = new QVBoxLayout(statsBox);
  statsLayout->addWidget(new QLabel("Result of last operation"));
  set_result = new QLabel("N/A");
  statsLayout->addWidget(set_result);
  statsLayout->addStretch();

  auto *parentLayout = new QHBoxLayout(w);
  parentLayout->addLayout(opLayout);
  parentLayout->addWidget(statsBox);
  return w;
}

QWidget *MainWindow::createRightPanel() {
  QWidget *w = new QWidget;
  w->setMinimumWidth(500);
  w->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);

  auto *layout = new QHBoxLayout(w);
  get_key_input = new QLineEdit;
  get_submit_button = new QPushButton("Submit");
  layout->addWidget(get_key_input);
  layout->addWidget(get_submit_button);

  auto *statsBox = new QGroupBox("Get Operation Statistics");
  auto *statsLayout = new QVBoxLayout(statsBox);
  get_result = new QLabel;
  statsLayout->addWidget(new QLabel("Result of last operation"));
  statsLayout->addWidget(get_result);

  layout->addWidget(statsBox);
  return w;
}

/**
 * @brief This function is resposible for the creation of all gui widgets and
 * layouts.
 */
void MainWindow::setupUi() {
  QWidget *central = new QWidget(this);
  setCentralWidget(central);

  auto *mainV = new QVBoxLayout(central);

  // optional top widget:
  // mainV->addWidget(new QLabel("Top area"));

  auto *h = new QHBoxLayout;
  h->addWidget(createLeftPanel());
  h->addWidget(createRightPanel());
  mainV->addLayout(h);

  // bottom area
  auto *bottom = new QWidget;
  auto *bottomLayout = new QHBoxLayout(bottom);
  bottomLayout->addWidget(new QLabel("Test of bottom text"));
  mainV->addWidget(bottom);
}

/**
 * @brief This function connects all gui buttons and their actions to handlers.
 *
 */
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
