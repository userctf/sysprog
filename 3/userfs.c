#include "userfs.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>


#define DESC_INIT_SIZE 10
enum {
  BLOCK_SIZE = 512,
  MAX_FILE_SIZE = 1024 * 1024 * 100,
};

/** Global error code. Set from any function on any error. */
static enum ufs_error_code ufs_error_code = UFS_ERR_NO_ERR;

struct block {
  /** Block memory. */
  char *memory;
  /** How many bytes are occupied. */
  size_t occupied;
  /** Next block in the file. */
  struct block *next;
  /** Previous block in the file. */
  struct block *prev;

  /* PUT HERE OTHER MEMBERS */
};

struct file {
  /** Double-linked list of file blocks. */
  struct block *block_list;
  /**
   * Last block in the list above for fast access to the end
   * of file.
   */
  struct block *last_block;
  /** How many file descriptors are opened on the file. */
  int refs;
  /** File name. */
  char *name;
  /** Files are stored in a double-linked list. */
  struct file *next;
  struct file *prev;

  /* PUT HERE OTHER MEMBERS */
  int is_deleted;
};

/** Deque of files */
static struct file *first_file = NULL;
static struct file *last_file = NULL;

struct filedesc {
  struct file *file;

  /* PUT HERE OTHER MEMBERS */
  size_t pos;
};

/**
 * An array of file descriptors. When a file descriptor is
 * created, its pointer drops here. When a file descriptor is
 * closed, its place in this array is set to NULL and can be
 * taken by next ufs_open() call.
 */
static struct filedesc **file_descriptors = NULL;
static int file_descriptor_count = 0;
static int file_descriptor_capacity = 0;

void init_descriptors(size_t initialSize) {
  file_descriptors = calloc(initialSize, sizeof(struct filedesc *));
  file_descriptor_count = 0;
  file_descriptor_capacity = initialSize;
}

void insert_descriptor(struct filedesc *element) {
  if (file_descriptor_count == file_descriptor_capacity) {
    file_descriptor_capacity *= 2;
    file_descriptors =
        realloc(file_descriptors,
                file_descriptor_capacity * // TODO: it doesn't fill with zeros
                    sizeof(struct filedesc *));
  }
  file_descriptors[file_descriptor_count++] = element;
}

void free_descriptors() {
  free(file_descriptors);
  file_descriptors = NULL;
  file_descriptor_count = file_descriptor_capacity = 0;
}

enum ufs_error_code ufs_errno() { return ufs_error_code; }

//////////////////////////////////////////////////// private part of realisation
struct block *create_new_block() {
  char *mem = malloc(BLOCK_SIZE * sizeof(char));
  if (mem == NULL) {
    return NULL;
  }

  struct block *new_block = (struct block *)malloc(sizeof(struct block));
  if (new_block == NULL) {
    return NULL;
  }

  new_block->memory = mem;
  new_block->occupied = 0;
  new_block->next = NULL;
  new_block->prev = NULL;

  return new_block;
}

void free_block() { ufs_error_code = UFS_ERR_NOT_IMPLEMENTED; }

int create_new_file(const char *filename) {
  struct file *new_file = (struct file *)malloc(sizeof(struct file));
  if (new_file == NULL) {
    ufs_error_code = UFS_ERR_NO_MEM;
    return -1;
  }

  char *filename_copy = (char *)malloc(strlen(filename) + 1);
  if (filename_copy == NULL) {
    ufs_error_code = UFS_ERR_NO_MEM;
    free(new_file);
    return -1;
  }

  struct block *new_block = create_new_block();
  if (new_block == NULL) {
    ufs_error_code = UFS_ERR_NO_MEM;
    free(filename_copy);
    free(new_file);
    return -1;
  }

  strcpy(filename_copy, filename);
  new_file->name = filename_copy;
  new_file->is_deleted = 0;
  new_file->refs = 0;
  new_file->block_list = new_block;
  new_file->last_block = new_block;

  new_file->next = NULL;
  if (first_file == NULL && last_file == NULL) {
    new_file->prev = NULL;
    first_file = last_file = new_file;
  } else {
    last_file->next = new_file;
    new_file->prev = last_file;
    last_file = new_file;
  }
  return 1;
}

void total_delete_file(struct file *current_file) {
  free(current_file->name);
  struct block *current_block = current_file->block_list;
  while (current_block != NULL) {
    free(current_block->memory);
    struct block *tmp = current_block->next;
    free(current_block);
    current_block = tmp;
  }
  free(current_file);
}

struct file *get_file(const char *filename) {
  struct file *current_file = first_file;
  if (current_file == NULL) {
    return NULL;
  }

  struct file *found = NULL;
  while (current_file != NULL) {
    if (current_file->is_deleted == 0 &&
        strcmp(current_file->name, filename) == 0) {
      found = current_file;
      break;
    }
    current_file = current_file->next;
  }
  if (found == NULL) {
    return NULL;
  }

  return found;
}
//////////////////////////////////////////////////// private part of realisation

int ufs_open(const char *filename, int flags) {
  // find (or create) file
  struct file *current_file = get_file(filename);
  if (current_file == NULL && flags & UFS_CREATE) {
    if (create_new_file(filename) != -1) {
      current_file = last_file;
    } else {
      ufs_error_code = UFS_ERR_NO_FILE;
      return -1;
    }
  } else if (current_file == NULL) {
    ufs_error_code = UFS_ERR_NO_FILE;
    return -1;
  }

  // create file_descriptor
  struct filedesc *file_desc =
      (struct filedesc *)malloc(sizeof(struct filedesc));
  if (file_desc == NULL) {
    ufs_error_code = UFS_ERR_NO_MEM;
    return -1;
  }
  file_desc->file = current_file;
  file_desc->pos = 0;
  file_desc->file->refs++;

  // locate file_descriptor
  if (file_descriptors == NULL) {
    init_descriptors(DESC_INIT_SIZE);
  }
  int found_space = -1;
  for (int i = 0; i < file_descriptor_count; ++i) {
    if (file_descriptors[i] == NULL) {
      file_descriptors[i] = file_desc;
      found_space = i;
      break;
    }
  }
  if (found_space == -1) {
    found_space = file_descriptor_count;
    insert_descriptor(file_desc);
  }

  return found_space;
}

ssize_t ufs_write(int fd, const char *buf, size_t size) {
  if (file_descriptors == NULL || file_descriptor_capacity <= fd || fd < 0) {
    ufs_error_code = UFS_ERR_NO_FILE;
    return -1;
  }

  struct filedesc *file_desc = file_descriptors[fd];
  if (file_desc == NULL) {
    ufs_error_code = UFS_ERR_NO_FILE;
    return -1;
  }
  struct block *current_block = file_desc->file->block_list; // rewrite
  int block_id = file_desc->pos / BLOCK_SIZE;
  size_t offset = file_desc->pos % BLOCK_SIZE;
  for (int i = 0; i < block_id; ++i) {
    current_block = current_block->next;
    if (current_block == NULL) {
      return 0;
    }
  }

  int size_to_write = BLOCK_SIZE - offset >= size ? size : BLOCK_SIZE - offset;
  ssize_t total_written = 0;
  while (size_to_write > 0) {
    if (block_id >= MAX_FILE_SIZE / BLOCK_SIZE) {
      ufs_error_code = UFS_ERR_NO_MEM;
      return -1;
    }
    memcpy((char *)current_block->memory + offset, buf + total_written,
           size_to_write);
    current_block->occupied = current_block->occupied > (offset + size_to_write)
                                  ? current_block->occupied
                                  : (offset + size_to_write);
    offset += size_to_write;
    total_written += size_to_write;
    size -= size_to_write;

    if (current_block->occupied == BLOCK_SIZE) {
      if (current_block->next == NULL) {
        struct block *new_block = create_new_block();
        if (new_block == NULL) {
          ufs_error_code = UFS_ERR_NO_MEM;
          return -1;
        }
        new_block->prev = current_block;
        current_block->next = new_block;
        file_desc->file->last_block = new_block;
        current_block = new_block;
      } else {
        current_block = current_block->next;
      }
      block_id++;
      offset = 0;
    }

    size_to_write = BLOCK_SIZE - offset >= size ? size : BLOCK_SIZE - offset;
  }
  file_desc->pos = BLOCK_SIZE * block_id + offset;
  return total_written;
}

ssize_t ufs_read(int fd, char *buf, size_t size) {
  if (file_descriptors == NULL || file_descriptor_capacity <= fd || fd < 0) {
    ufs_error_code = UFS_ERR_NO_FILE;
    return -1;
  }

  struct filedesc *file_desc = file_descriptors[fd];
  if (file_desc == NULL) {
    ufs_error_code = UFS_ERR_NO_FILE;
    return -1;
  }

  int block_id = file_desc->pos / BLOCK_SIZE;
  size_t offset = file_desc->pos % BLOCK_SIZE;
  struct block *current_block = file_desc->file->block_list; // first block;
  for (int i = 0; i < block_id; ++i) {
    current_block = current_block->next;
    if (current_block == NULL) {
      return 0;
    }
  }

  int size_to_read = current_block->occupied - offset >= size
                         ? size
                         : current_block->occupied - offset;
  ssize_t total_read = 0;
  while (size > 0) {
    memcpy(buf + total_read, (char *)current_block->memory + offset,
           size_to_read);
    offset += size_to_read;
    total_read += size_to_read;
    size -= size_to_read;

    if (current_block->occupied == offset) {
      if (offset < BLOCK_SIZE ||
          (offset == BLOCK_SIZE &&
           current_block->next == NULL)) { // no data left
        file_desc->pos = BLOCK_SIZE * block_id + offset;
        return total_read;
      } else if (offset == BLOCK_SIZE) {
        block_id += 1;
        offset = 0;
        current_block = current_block->next;
      }
    }

    size_to_read = current_block->occupied - offset >= size
                       ? size
                       : current_block->occupied - offset;
  }
  file_desc->pos = BLOCK_SIZE * block_id + offset;
  return total_read;
}

int ufs_close(int fd) {
  if (file_descriptors == NULL || file_descriptor_capacity <= fd || fd < 0) {
    ufs_error_code = UFS_ERR_NO_FILE;
    return -1;
  }

  struct filedesc *file_desc = file_descriptors[fd];
  if (file_desc == NULL) {
    ufs_error_code = UFS_ERR_NO_FILE;
    return -1;
  }
  struct file *currect_file = file_desc->file;
  currect_file->refs--;
  if (currect_file->is_deleted == 1 && currect_file->refs == 0) {
    total_delete_file(currect_file);
  }

  file_descriptors[fd] = NULL;
  free(file_desc);

  return 0;
}

int ufs_delete(const char *filename) {
  struct file *current_file = get_file(filename);
  if (current_file == NULL) {
    ufs_error_code = UFS_ERR_NO_FILE;
    return -1;
  }

  // remove from file's deque
  if (first_file == current_file && last_file == current_file) {
    first_file = NULL;
    last_file = NULL;
  } else if (first_file == current_file) {
    first_file = current_file->next;
  } else if (last_file == current_file) {
    last_file = current_file->prev;
  } else {
    current_file->prev->next = current_file->next;
    current_file->next->prev = current_file->prev;
  }

  // current_file->is_deleted = 1;

  if (current_file->refs == 0 &&
      current_file->is_deleted == 1) { // clear memory
    total_delete_file(current_file);
  }
  return 0;
}

#if NEED_RESIZE

int ufs_resize(int fd, size_t new_size) {
  /* IMPLEMENT THIS FUNCTION */
  (void)fd;
  (void)new_size;
  ufs_error_code = UFS_ERR_NOT_IMPLEMENTED;
  return -1;
}

#endif

void ufs_destroy(void) {

  // remove all files
  struct file *current_file = first_file;
  while (current_file != NULL) {
    struct file *tmp = current_file->next;
    total_delete_file(current_file);
    current_file = tmp;
  }

  // remove all descriptors
  for (int i = 0; i < file_descriptor_capacity; ++i) {
    if (file_descriptors[i] != NULL) {
      free(file_descriptors[i]);
    }
  }
  free_descriptors();
}
