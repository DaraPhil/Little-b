#include<stdio.h>
#include<string.h>

double results;
double num1;
double num2;
double var1;
double var2;
char operation[20];
int choice;

double Calc(double num1, double num2) {
    printf("Enter your first variable: \n");
    scanf("%lf", &num1);

    printf("Enter your second variable: \n");
    scanf("%lf", &num2);

    printf("What operation do you want to perform\n ");
    printf("1-Addition \n 2-Subtraction \n 3-Multiplication \n 4-Division\n");
    printf("Choose a coresponding number: ");
    scanf("%d", &choice);


    if(choice==1) {
        strcpy(operation,"addition");
        results = num1 + num2;
    }
    else if(choice==2) {
        strcpy(operation,"subtraction");
        results = num1-num2;
    }
    else if(choice==3) {
        strcpy(operation,"multiplication");
        results = num1 * num2;
    }
    else if (choice==4) {
        strcpy(operation, "division");
        results = num1/num2;
    }
    else {
        printf("Invalid selection ");
        return 0.0;
    }
    printf("You chose %s and the solution is ", operation);

    return results;

}

void main()
{
    printf("%.4f",Calc(num1, num2));
}