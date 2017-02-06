#include "irc.h"
#include "cal.h"

extern char buffer[BUFFER_SIZE];
extern char operator_stack[MAX_STACK_SIZE];
extern double number_stack[MAX_STACK_SIZE];
extern int operator_stack_size;
extern int number_stack_size;
extern int calculator_error; 

int precedence(char c)
{
    if(c == '^')
        return 1;
    if(c == '*' || c == '/')
        return 2;
    if(c == '+' || c == '-')
        return 3;
    if(c == '(')
        return 4;
    return 0;
}

void calculate(char c)
{
    double b = number_stack[number_stack_size - 1];
    number_stack[number_stack_size - 1] = 0;
    number_stack_size--;
    double a = number_stack[number_stack_size - 1];
    number_stack[number_stack_size - 1] = 0;
    number_stack_size--;
    double result;  
    if(c == '+')
        result = a + b;
    if(c == '-')
        result = a - b;
    if(c == '*')
        result = a * b;
    if(c == '/')
    {
        if(b == 0)
        {
            calculator_error = DIVIDE_ZERO;
            return;
        }
        result = a / b;
    }
    if(c == '^') // pow
    {
        result = 1;
        int power = (int)(b + 1E-7); // add 1E-7 to avoid rounding error
        int i;
        for(i = 1; i <= power; i++)
            result *= a;
    }
    if(c == '(')
    {
        calculator_error = UNMATCHED_PARENTHESES;
        return;
    }
    number_stack[number_stack_size] = result;
    number_stack_size++;
}

void operator_push(char c)
{
    char top;
    int p;
    if(c == ')')
    {
        while(operator_stack_size > 0 && operator_stack[operator_stack_size - 1] != '(')
        {
            if(calculator_error)
                return;
            calculate(operator_stack[operator_stack_size - 1]);
            operator_stack[operator_stack_size - 1] = 0;
            operator_stack_size--;
        }
        if(operator_stack_size == 0)
        {
            calculator_error = UNMATCHED_PARENTHESES;
            return;
        }
        operator_stack[operator_stack_size - 1] = 0;
        operator_stack_size--;
    }
    else
    {
        p = precedence(c);
        while(operator_stack_size > 0 && precedence(operator_stack[operator_stack_size - 1]) <= p)
        {
            if(calculator_error)
                return;
            calculate(operator_stack[operator_stack_size - 1]);
            operator_stack[operator_stack_size - 1] = 0;
            operator_stack_size--;
        }
        operator_stack[operator_stack_size] = c;
        operator_stack_size++;
    }
}

void operator_clear()
{
    while(operator_stack_size > 0)
    {
        if(calculator_error)
            return;
        calculate(operator_stack[operator_stack_size - 1]);
        operator_stack[operator_stack_size - 1] = 0;
        operator_stack_size--;
    }
}

void number_push(double num)
{
    number_stack[number_stack_size] = num;
    number_stack_size++;
}

void character_switch(char c, int *pushed, int *dot, double *num, double *digit_now, int *waiting_num)
{
    if('0' <= c && c <= '9')
    {
        if((*waiting_num) == 0) // input is expected to be an operator
        {
            calculator_error = ILLEGAL_EXPRESSION;
            return;
        }
        if(*dot)
        {
            (*digit_now) /= 10;
            (*num) += (*digit_now) * (c - '0');
        }
        else
        {
            (*pushed) = 0;
            (*num) *= 10;
            (*num) += (c - '0');
        }
    }
    else if(c == '.')
    {
        if(*pushed) // this is an extra .
        {
            calculator_error = ILLEGAL_EXPRESSION;
            return;
        }
        (*dot) = 1;
        (*digit_now) = 1;
    }
    else if(c == '(')
    {
        if((*pushed) == 0 || (*waiting_num) == 0) // input is expected to be an operator
        {
            calculator_error = ILLEGAL_EXPRESSION;
            return;
        }
        operator_stack[operator_stack_size] = c;
        operator_stack_size++;
    }
    else if(c == ')')
    {
        if(!(*pushed))
        {
            number_push(*num);
            (*pushed) = 1;
            (*waiting_num) = 0;
            (*num) = 0;
            (*dot) = 0;
        }
        if((*waiting_num) == 1) // input is expected to be a number
        {
            calculator_error = ILLEGAL_EXPRESSION;
            return;
        }
        operator_push(c); 
    }
    else if(c == '+' || c == '-' || c == '*' || c == '/' || c == '^')
    {
        if(!(*pushed))
        {
            number_push(*num);
            (*pushed) = 1;
            (*waiting_num) = 0;
            (*num) = 0;
            (*dot) = 0;
        }
        if((*waiting_num) == 1) // input is expected to be an number
        {
            calculator_error = ILLEGAL_EXPRESSION;
            return;
        }
        operator_push(c);
        (*waiting_num) = 1;
    }
    else if(c == ' ')
    {
        if(!(*pushed))
        {
            number_push(*num);
            (*pushed) = 1;
            (*waiting_num) = 0;
            (*num) = 0;
            (*dot) = 0;
        }
    }
    else
    {
        calculator_error = UNEXPECTED_CHARACTER;
        return;
    }
}

double calculator(int *now_index)
{
    char c;
    double num = 0;
    double digit_now = 1;
    int pushed = 1; // pushed = 0 if current number is still not pushed into the stack 
    int waiting_num = 1; // next input should be a number
    int dot = 0;
    while(buffer[(*now_index)] != '\r')
    {         
        if(calculator_error == 0)
            character_switch(buffer[(*now_index)], &pushed, &dot, &num, &digit_now, &waiting_num);
        (*now_index)++;
    }
    (*now_index) += 2; // skip \r \n
    if(calculator_error == 0)
    {
        if(!pushed)
            number_push(num);
        operator_clear();
        return number_stack[0];
    }
}
