#include "fs_simulator.h"

#define MAX_INODES 1024
static char inode_usage[MAX_INODES] = {0};

ssize_t safe_strscpy(char *dest, const char *src, size_t count) {
    if (count == 0)
        return -E2BIG;

    size_t i;

    for (i = 0; i < count - 1 && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';

    return src[i] == '\0' ? i : -E2BIG;
}

void initialize_inode_usage(void) {
    memset(inode_usage, 0, sizeof(inode_usage)); 

    char path[256];

    for (uint32_t i = 0; i < MAX_INODES; i++) {
        sprintf(path, "fs/%d", i);
        if (access(path, F_OK) != -1) { 
            inode_usage[i] = 1; 
        }
    }
}

uint32_t get_next_inode_number(void) {
    for (uint32_t i = 1; i < MAX_INODES; i++) { 
        if (inode_usage[i] == 0) {
            inode_usage[i] = 1;
            return i;
        }
    }
    return UINT32_MAX;
}

void list_directory_contents(void) {
    char directory_name[256];

    sprintf(directory_name, "fs/%d", currentDirectoryInode);

    int fd = open(directory_name, O_RDONLY);

    if (fd == -1) {
        perror("Failed to open directory");
        return;
    }

    DirectoryEntry entry;

    while (read(fd, &entry, sizeof(DirectoryEntry)) == sizeof(DirectoryEntry)) {
        printf("%u %s\n", entry.inode_number, entry.name);
    }

    close(fd);
}


void change_directory(const char *directory_name) {
    char directory_path[256];

    sprintf(directory_path, "fs/%d", currentDirectoryInode);

    int fd = open(directory_path, O_RDONLY);

    if (fd == -1) {
        perror("Failed to open directory");
        return;
    }

    DirectoryEntry entry;

    int found = 0;
    int is_directory = 0;

    while (read(fd, &entry, sizeof(DirectoryEntry)) == sizeof(DirectoryEntry)) {
        if (strcmp(entry.name, directory_name) == 0) {
            found = 1;
            break;
        }
    }
    close(fd);
    if (!found) {
        printf("Directory not found\n");
    } else {
        currentDirectoryInode = entry.inode_number;
    } 

}


void create_directory(const char *dir_name) {
    char current_dir_path[256];

    sprintf(current_dir_path, "fs/%d", currentDirectoryInode);

    int dir_fd = open(current_dir_path, O_RDWR);

    if (dir_fd == -1) {
        perror("Failed to open current directory");
        return;
    }

    DirectoryEntry entry;
    
    while (read(dir_fd, &entry, sizeof(DirectoryEntry)) == sizeof(DirectoryEntry)) {
        if (strcmp(entry.name, dir_name) == 0) {

            close(dir_fd);
            return;
        }
    }

    int new_inode = get_next_inode_number();

    if (new_inode == UINT32_MAX || new_inode <= 6) {
        printf("No suitable inodes available\n");
        close(dir_fd);
        return;
    }

    DirectoryEntry new_entry = {new_inode, 'd'};

    safe_strscpy(new_entry.name, dir_name, sizeof(new_entry.name) - 1);

    new_entry.name[sizeof(new_entry.name) - 1] = '\0';

    lseek(dir_fd, 0, SEEK_END);
    if (write(dir_fd, &new_entry, sizeof(DirectoryEntry)) != sizeof(DirectoryEntry)) {
        perror("Failed to write new directory entry");
        close(dir_fd);
        return;
    }
    close(dir_fd);

    char new_directory_path[256];

    sprintf(new_directory_path, "fs/%d", new_inode);

    int new_dir_fd = open(new_directory_path, O_RDWR | O_CREAT, 0777);

    if (new_dir_fd == -1) {
        perror("Failed to create directory file");
        return;
    }

    DirectoryEntry dot_entry = {new_inode, 'd'};

    strcpy(dot_entry.name, ".");

    DirectoryEntry dotdot_entry = {currentDirectoryInode, 'd'};

    strcpy(dotdot_entry.name, "..");

    if (write(new_dir_fd, &dot_entry, sizeof(DirectoryEntry)) != sizeof(DirectoryEntry) ||
        write(new_dir_fd, &dotdot_entry, sizeof(DirectoryEntry)) != sizeof(DirectoryEntry)) {
        perror("Failed to initialize new directory");
    }
    close(new_dir_fd);

    printf("Directory '%s' created with inode %d\n", dir_name, new_inode);
}


void touch_file(const char *filename) {
    char directory_path[256];

    sprintf(directory_path, "fs/%d", currentDirectoryInode);
    
    int dir_fd = open(directory_path, O_RDWR);

    if (dir_fd == -1) {
        perror("Failed to open directory");
        return;
    }

    DirectoryEntry entry;

    int found = 0;

    int inode_number = -1;
    

    while (read(dir_fd, &entry, sizeof(DirectoryEntry)) == sizeof(DirectoryEntry)) {
        if (strcmp(entry.name, filename) == 0) {
            found = 1;
            inode_number = entry.inode_number;
            break;
        }
    }

    if (!found) {

        inode_number = get_next_inode_number();

        if (inode_number == UINT32_MAX) {
            printf("No inodes available\n");
            close(dir_fd);
            return;
        }

        DirectoryEntry new_entry = {inode_number, 'f'};

        safe_strscpy(new_entry.name, filename, sizeof(new_entry.name) - 1);

        new_entry.name[sizeof(new_entry.name) - 1] = '\0';
        
        lseek(dir_fd, 0, SEEK_END);

        if (write(dir_fd, &new_entry, sizeof(DirectoryEntry)) != sizeof(DirectoryEntry)) {
            perror("Failed to write new file entry");
            close(dir_fd);
            return;
        }
    }

    close(dir_fd);

    char file_path[256];
    
    sprintf(file_path, "fs/%d", inode_number);

    int file_fd = open(file_path, O_RDWR | O_CREAT, 0666);

    if (file_fd == -1) {
        perror("Failed to open or create file");
        return;
    }

    close(file_fd);

    if (!found) {
        printf("File '%s' created with inode %d\n", filename, inode_number);
    } else {

    }
}


int verify_root_directory(void) {
    int fd = open("fs/inodes_list", O_RDONLY);

    if (fd == -1) {
        perror("Failed to open inode list");
        return -1;
    }

    InodeEntry inode;

    if (read(fd, &inode, sizeof(InodeEntry)) != sizeof(InodeEntry)) {
        perror("Failed to read inode");
        close(fd);
        return -1;
    }
    if (inode.type != 'd' || inode.inode_number != 0) {
        printf("Root inode is not a directory\n");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}


int main(void) {
    initialize_inode_usage();

    if (verify_root_directory() == -1) {
        return 0;
    }

    int fd = open("fs/inodes_list", O_RDONLY);

    if (fd == -1) {
        perror("Failed to open file");
        return 1;
    }

    char command[256];

    while (1) {

        printf("> ");
        if (fgets(command, 256, stdin) == NULL) {
            break;
        }

        command[strcspn(command, "\n")] = 0;

        if (strcmp(command, "exit") == 0) {
            break;
        } else if (strcmp(command, "ls") == 0) {
            list_directory_contents();
        } else if (strncmp(command, "cd ", 3) == 0) {
            change_directory(command + 3);
        } else if (strncmp(command, "mkdir ", 6) == 0) {
            create_directory(command + 6);
        } else if (strncmp(command, "touch ", 6) == 0) {
            touch_file(command + 6);
        } else {
            printf("Unknown command\n");
        }
    }
    close(fd);
    return 0;
}
