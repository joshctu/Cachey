#pragma once
#include "store_controller.hpp"
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(StoreController *storeController,
                      QWidget *parent = nullptr);
  ~MainWindow() = default;

private:
  StoreController *storeController;

  QLineEdit *set_key_input;
  QLineEdit *set_value_input;
  QPushButton *set_submit_button;
  QPushButton *delete_submit_button;
  QLabel *set_result;
  QLabel *set_timer_results;
  QPushButton *seed_entries_button;
  QLabel *total_records;

  QPushButton *get_submit_button;
  QLineEdit *get_key_input;
  QLabel *get_result;
  QLabel *get_timer_results;
  QLabel *dead_bytes_results;

  QWidget *createLeftPanel();
  QWidget *createRightPanel();
  void onSetKey();
  void onGetKey();
  void onDeleteKey();
  void onCompactionComplete(const int segments_merged,
                            const uint16_t records_removed);
  void onSeedEntries();
  void setupUi();
  void initStore();
  void setupConnections();
};
