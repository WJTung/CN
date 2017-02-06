#include <stdio.h>

#define MAX_STACK_SIZE 100
#define UNMATCHED_PARENTHESES 1
#define UNEXPECTED_CHARACTER 2
#define DIVIDE_ZERO 3
#define ILLEGAL_EXPRESSION 4

int precedence(char c);
void calculate(char c);
void operator_push(char c);
void operator_clear();
void number_push(double num);
void character_switch(char c, int *pushed, int *dot, double *num, double *digit_now, int *waiting_num);
double calculator(int *now_index);
