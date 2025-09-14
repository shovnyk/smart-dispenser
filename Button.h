#pragma once

enum button_type {
  BUTTON_A,
  BUTTON_B,
  BUTTON_NONE
};

enum button_type get_user_input(bool block);

void read_buttons(int *btnA, int *btnB);

void button_init();
