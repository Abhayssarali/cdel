#include <stdio.h>

int compute(int a, int b) {

    // Useful instruction
    int x = a + b;

    // Dead instructions
    int d1 = a * 100;
    int d2 = d1 + 50;
    int d3 = d2 / 2;

    // Useful instruction
    int y = x * 2;

    // Dead chain
    int temp1 = a - b;
    int temp2 = temp1 * 5;
    int temp3 = temp2 + 10;

    // Useful branch
    if (y > 50) {

        // Dead inside branch
        int branchDead1 = y * 100;
        int branchDead2 = branchDead1 + 1;

        printf("Large value\n");
    }

    // Useful instruction
    int z = y + 10;

    // Dead assignment overwrite
    int overwrite = 5;
    overwrite = 10;

    // Useful output
    return z;
}

int main() {
    int result = compute(20, 15);
    printf("Result = %d\n", result);
    return 0;
}
