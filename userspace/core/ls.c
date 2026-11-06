#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE 1024
#define I_DIR  40000
#define I_FILE 100000
#define I_DEV  400000

struct dirent { 
    unsigned long long i_no;
    char i_name[257];
    int i_type;
    unsigned long long i_size;
    
};
int main(int argc, char *argv[]) {
    FILE *file;
    char *types[] = {"FILE","DIR","DEV"};
    char *ctype;
	struct dirent *dirs = malloc(sizeof(struct dirent)*1024);

    if (argc == 2) { 
        file = fopen(argv[1],"r");
    } else {
        file = fopen(".","r");
    }
    
    if (file == NULL) {
        fprintf(stderr, "Could not open file:");
        exit(EXIT_FAILURE);
    }

    int retcall = getdents(fileno(file),dirs,1024);
    fclose(file);
    if (retcall <1) {
        fprintf(stderr, "ls <dir>\n");
        exit(EXIT_FAILURE);
    }
    // Print directory
    for (int i=0; i<retcall; i++) 
    {
        ctype = types[2];
        if (dirs[i].i_type == I_DIR)
        {
            ctype = types[1];
        } else if (dirs[i].i_type == I_FILE) {
            ctype = types[0];
        }
        printf("%-20s%-20s%10d\n", dirs[i].i_name, ctype,
               (int)dirs[i].i_size);
    }
    return 0;
}
