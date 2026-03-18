#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

typedef struct proc {
    char parent[256];
    char name[256];
    int priority;
    int memory;
} proc;

typedef struct proc_tree {
    proc data;
    struct proc_tree *left;
    struct proc_tree *right;
} proc_tree;

/* Create node */
proc_tree *create_node(proc p) {
    proc_tree *node = (proc_tree *)malloc(sizeof(proc_tree));
    if (node == NULL) {
        printf("Memory allocation failed.\n");
        exit(1);
    }

    node->data = p;
    node->left = NULL;
    node->right = NULL;
    return node;
}

/* Find node by name */
proc_tree *find_node(proc_tree *root, const char *name) {
    if (root == NULL) return NULL;

    if (strcmp(root->data.name, name) == 0) {
        return root;
    }

    proc_tree *found = find_node(root->left, name);
    if (found != NULL) return found;

    return find_node(root->right, name);
}

/* Add child */
bool add_child(proc_tree *root, const char *parent_name, proc child_data) {
    proc_tree *parent_node = find_node(root, parent_name);

    if (parent_node == NULL) return false;

    if (parent_node->left == NULL) {
        parent_node->left = create_node(child_data);
        return true;
    } else if (parent_node->right == NULL) {
        parent_node->right = create_node(child_data);
        return true;
    } else {
        printf("Parent %s already has two children.\n", parent_name);
        return false;
    }
}

/* CLEAN PRINT FUNCTION (what your TA wants) */
void print_tree(proc_tree *root, int level) {
    if (root == NULL) return;

    for (int i = 0; i < level; i++) {
        printf("  ");  // indentation
    }

    printf("%s\n", root->data.name);

    print_tree(root->left, level + 1);
    print_tree(root->right, level + 1);
}

/* Free memory */
void free_tree(proc_tree *root) {
    if (root == NULL) return;

    free_tree(root->left);
    free_tree(root->right);
    free(root);
}

int main(void) {
    FILE *file = fopen("process_tree.txt", "r");
    if (file == NULL) {
        printf("Could not open process_tree.txt\n");
        return 1;
    }

    proc processes[100];
    int count = 0;
    char line[1024];

    /* Read file */
    while (fgets(line, sizeof(line), file) != NULL) {
        proc p;
        line[strcspn(line, "\n")] = '\0';

        if (sscanf(line, "%255[^,],%255[^,],%d,%d",
                   p.parent, p.name, &p.priority, &p.memory) == 4) {
            processes[count++] = p;
        }
    }

    fclose(file);

    if (count == 0) {
        printf("No data found.\n");
        return 1;
    }

    proc_tree *root = NULL;

    /* Find root */
    for (int i = 0; i < count; i++) {
        if (strcmp(processes[i].parent, "none") == 0 ||
            strcmp(processes[i].parent, "NULL") == 0 ||
            strcmp(processes[i].parent, "-") == 0) {
            root = create_node(processes[i]);
            break;
        }
    }

    if (root == NULL) {
        root = create_node(processes[0]);
    }

    bool added[100] = {false};

    for (int i = 0; i < count; i++) {
        if (strcmp(root->data.name, processes[i].name) == 0) {
            added[i] = true;
            break;
        }
    }

    bool progress = true;
    while (progress) {
        progress = false;

        for (int i = 0; i < count; i++) {
            if (!added[i]) {
                if (find_node(root, processes[i].parent) != NULL) {
                    if (add_child(root, processes[i].parent, processes[i])) {
                        added[i] = true;
                        progress = true;
                    }
                }
            }
        }
    }

    printf("Process Tree:\n");
    print_tree(root, 0);

    free_tree(root);
    return 0;
}
