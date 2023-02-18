#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include<fcntl.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <ftw.h>
#include <grp.h>
#include <pwd.h>

#define BLOCK_SIZE 512
#define BUFFER_SIZE 1024

#define MAX_PATH_LENGTH 256
char *file_name_to_create,*file_directory;

enum Header {
	NAME = 0, MODE = 100, UID = 108, GID = 116, SIZE = 124, MTIME = 136,
	CHK = 148, TYPE = 156, LINK = 157, VERS = 263, UNAME = 265,
	GNAME = 297, MAJ = 329, MIN = 337, END = 512
};

enum Type {
	REG = '0', HARDLINK = '1', SYMLINK = '2', CHARDEV = '3', BLOCKDEV = '4',
	DIRECTORY = '5', FIFO = '6'
};

void chksum(const char b[END], char *chk) {
	unsigned sum = 0, i;
	for(i = 0; i<END; i++)
		sum += (i >= CHK && i < CHK+8) ? ' ' : b[i];
	snprintf(chk, 8, "%.7o", sum);
}

static int
parseoct(const char *p, size_t n)
{
	int i = 0;

	while ((*p < '0' || *p > '7') && n > 0) {
		++p;
		--n;
	}
	while (*p >= '0' && *p <= '7' && n > 0) {
		i *= 8;
		i += *p - '0';
		++p;
		--n;
	}
	return (i);
}

static void
create_dir(char *pathname, int mode)
{
	char *p;
	int r;

	/* Strip trailing '/' */
	if (pathname[strlen(pathname) - 1] == '/')
		pathname[strlen(pathname) - 1] = '\0';
        //pathname="testDump";
	/* Try creating the directory. */
#if defined(_WIN32) && !defined(__CYGWIN__)
	r = _mkdir(pathname);
#else
	r = mkdir(pathname, mode);
#endif

	if (r != 0) {
		/* On failure, try creating parent directory. */
		p = strrchr(pathname, '/');
		if (p != NULL) {
			*p = '\0';
			create_dir(pathname, 0755);
			*p = '/';
#if defined(_WIN32) && !defined(__CYGWIN__)
			r = _mkdir(pathname);
#else
			r = mkdir(pathname, mode);
#endif
		}
	}
	if (r != 0)
		fprintf(stderr, "Could not create directory %s\n", pathname);
}

void remove_tar_extension(char *str) {
  int len = strlen(str);
  if (len >= 4 && strcmp(str + len - 4, ".tar") == 0) {
    str[len - 4] = '\0';
  }
}

static FILE *
create_file(char *pathname, int mode)
{
	FILE *f;
	char *temp;
	char dir[100]="Dump/";
	char slash[100]="/";
	char *file_name = strrchr(pathname, '/');
	
	char file_directory_name[100];//=strremove(file_directory, ".tar");
	remove_tar_extension(file_directory);
        if (file_name == NULL) {
            file_name = pathname;
        } else {
            file_name++;
        }
        char dump_dest[100];
        sprintf(dump_dest,"%s%s",file_directory,dir);
        
	sprintf(temp,"%s%s",dump_dest,file_name);
	
	//printf("%s\n",temp);
	pathname=temp;
	f = fopen(pathname, "wb+");
	if (f == NULL) {
		/* Try creating parent dir and then creating file. */
		char *p = strrchr(pathname, '/');
		if (p != NULL) {
			*p = '\0';
			create_dir(pathname, 0755);
			*p = '/';
			f = fopen(pathname, "wb+");
		}
	}
	return (f);
}



static int
verify_checksum(const char *p)
{
	int n, u = 0;
	for (n = 0; n < 512; ++n) {
		if (n < 148 || n > 155)
			/* Standard tar checksum adds unsigned bytes. */
			u += ((unsigned char *)p)[n];
		else
			u += 0x20;

	}
	return (u == parseoct(p + 148, 8));
}

static void
untar(FILE *a, const char *path)
{
	char buff[512];
	FILE *f = NULL;
	size_t bytes_read;
	int filesize;

	//printf("Extracting from %s\n", path);
	for (;;) {
		bytes_read = fread(buff, 1, 512, a);
		if (bytes_read < 512) {
			/*fprintf(stderr,
			    "Short read on %s: expected 512, got %d\n",
			    path, (int)bytes_read);*/
			return;
		}
	
		int n,k;
	        for (n = 511; n >= 0; --n){
		    if (buff[n] != '\0')
			k+=1;
		}	
		if (k==0){
		    return;//eof
		}
		
		if (!verify_checksum(buff)) {
			fprintf(stderr, "Checksum failure\n");
			return;
		}
		filesize = parseoct(buff + 124, 12);
		//printf("test buff vlaue at 156 %d\n",buff[156]);
		switch (buff[156]) {
		case '1':
			//printf(" Ignoring hardlink %s\n", buff);
			break;
		case '2':
			//printf(" Ignoring symlink %s\n", buff);
			break;
		case '3':
			//printf(" Ignoring character device %s\n", buff);
				break;
		case '4':
			//printf(" Ignoring block device %s\n", buff);
			break;
		case '5':
			//printf(" Extracting dir %s\n", buff);
			create_dir(buff, parseoct(buff + 100, 8));
			filesize = 0;
			break;
		case '6':
			//printf(" Ignoring FIFO %s\n", buff);
			break;
		default:
			//printf(" Extracting file %s\n", buff);
			//char *temp;
			//char dir[100]="Dump/";
			//sprintf(temp,"%s",buff);
			//printf("%s\n",temp);
			//int k=mkdir("testDump",0755);
			//printf("direcotry");
	                
			f = create_file(buff, parseoct(buff + 100, 8));
			break;
		}
		while (filesize > 0) {
			bytes_read = fread(buff, 1, 512, a);
			if (bytes_read < 512) {
				/*fprintf(stderr,
				    "Short read on %s: Expected 512, got %d\n",
				    path, (int)bytes_read);*/
				return;
			}
			if (filesize < 512)
				bytes_read = filesize;
			if (f != NULL) {
				if (fwrite(buff, 1, bytes_read, f)
				    != bytes_read)
				{
					fprintf(stderr, "Failed write\n");
					fclose(f);
					f = NULL;
				}
			}
			filesize -= bytes_read;
		}
		if (f != NULL) {
			fclose(f);
			f = NULL;
		}
	}
}

static void
listtar(char *a, const char *path)
{
    int fd;
    char buffer[BLOCK_SIZE];
    int files_found = 0;

    

    if ((fd = open(a, O_RDONLY)) == -1) {
        printf("Error: unable to open tar archive %s\n", a);
        
    }

    struct stat st;
    stat(a, &st);
    int size_of_tar_file = st.st_size;
   
    char A[1000][1000];
    char B[1000][1000];
    while (1) {
        int bytes_read = read(fd, buffer, BLOCK_SIZE);
        if (bytes_read < BLOCK_SIZE) {
            break;
        }

        char filename[100];
        int filesize = 0;
        
        strncpy(filename, buffer, 100);

        sscanf(buffer + 124, "%12o", &filesize);

        if (filesize > 0) {
            char *file_name = strrchr(filename, '/');
            if (file_name == NULL) {
                file_name = filename;
            } else {
                file_name++;
            }
            //printf("File: %s, Size: %d\n", file_name, filesize);
            strcpy(A[files_found],file_name);
            char temp[1000];
            sprintf(temp,"%d",filesize);
            strcpy(B[files_found],temp);
            files_found++;
        }

        lseek(fd, (filesize + BLOCK_SIZE - 1) / BLOCK_SIZE * BLOCK_SIZE, SEEK_CUR);
    }
    

    close(fd);
    FILE *fptr;
    
    fptr=fopen("tarStructure","w");
    fprintf(fptr,"%d", size_of_tar_file);
    //printf("hellos");
    fprintf(fptr,"\n%d", files_found);
    for(int i=0;i<files_found;i++){
    fprintf(fptr,"\n%s %s",A[i],B[i]);
    int k=0;
    }
    fclose(fptr);
    if (files_found == 0) {
        printf("No files found in tar archive %s\n", a);
    }


}


int c_file(const char* path, const struct stat* st, int type) {
	int l = END;
	char b[END] = { 0 };
	FILE *f = NULL;
	struct passwd *pw = getpwuid(st->st_uid);
	struct group *gr = getgrgid(st->st_gid);
	FILE *file;
	char dir[100]="Dump/";
	char slash[100]="/";
	char file_directory_name[100];//=strremove(file_directory, ".tar");
	remove_tar_extension(file_directory);
        
        char dump_dest[100];
        sprintf(dump_dest,"%s%s%s",file_directory,slash,file_name_to_create);
        //printf("%s\n",dump_dest);
	file = fopen(file_name_to_create, "a+");
        
	memset(b+SIZE, '0', 12);
	strcpy(b+VERS, "00");
	snprintf(b+NAME, 100, "%s", path);
	snprintf(b+MODE, 8, "%.7o", (unsigned)st->st_mode&0777);
	snprintf(b+UID,  8, "%.7o", (unsigned)st->st_uid);
	snprintf(b+GID,  8, "%.7o", (unsigned)st->st_gid);
	snprintf(b+MTIME,12, "%.11o", (unsigned)st->st_mtime);
	snprintf(b+UNAME, 32, "%s", pw->pw_name);
	snprintf(b+GNAME, 32, "%s", gr->gr_name);	
	mode_t mode = st->st_mode;
	if(S_ISREG(mode)) {
		b[TYPE] = REG;
		snprintf(b+SIZE, 12, "%.11o", (unsigned)st->st_size);
		f = fopen(path, "r");
	} else if(S_ISDIR(mode)) {
		b[TYPE] = DIRECTORY;
	} else if(S_ISLNK(mode)) {
		b[TYPE] = SYMLINK;
		readlink(path, b+LINK, 99);
	} else if(S_ISCHR(mode)) {
		b[TYPE] = CHARDEV;
		snprintf(b+MAJ,  8, "%.7o", (unsigned)major(st->st_dev));
		snprintf(b+MIN,  8, "%.7o", (unsigned)minor(st->st_dev));
	} else if(S_ISBLK(mode)) {
		b[TYPE] = BLOCKDEV;
		snprintf(b+MAJ,  8, "%.7o", (unsigned)major(st->st_dev));
		snprintf(b+MIN,  8, "%.7o", (unsigned)minor(st->st_dev));
	} else if(S_ISFIFO(mode)) {
		b[TYPE] = FIFO;
	}
	chksum(b, b+CHK);
	if(!f) return 0;
	do {
		if(l<END)
			memset(b + l, 0, END - l);
		//fwrite(b, END, 1, stdout);
		fwrite(b, END, 1, file);
		//fprintf(file, END);
		//fclose(file);
	}
	while((l = fread(b, 1, END, f)) > 0);
	fclose(f);
	fclose(file);
	return 0;
}


int t(char *fname, int l, char b[END]){
	puts(fname);
	for(; l > 0; l -= END)
		fread(b, END, 1, stdin);
	return 0;
}


int c(char *p) {
	static struct stat st; 
	if(lstat(p, &st)){
		//perror(p);
		//printf("test");
		return c_file(p, &st, 0);
	}else if(S_ISDIR(st.st_mode)){
	        int temp=ftw(p, c_file, 1024);
	        //printf("%d\n",temp);
		return 0;
		//return ftw(p, c_file, 1024);
	}else
		return c_file(p, &st, 0);
	return 1;
}



int main(int argc, char *argv[]) {
	if (strcmp(argv[1], "-c") == 0)
	{
		file_name_to_create=argv[3];
		
		file_directory=argv[2];
		while(argc-- >= 3)
			if(c(argv[argc])) return EXIT_FAILURE;
			
	        FILE *source_file, *target_file;
    char buffer[BUFFER_SIZE];
    size_t bytes_read;

    source_file = fopen(file_name_to_create, "r");
    if (source_file == NULL) {
        printf("Unable to open source file\n");
        exit(EXIT_FAILURE);
    }
    char tempp[100];
    char splash[100]="/";
    sprintf(tempp,"%s%s%s",file_directory,splash,file_name_to_create);
    //printf("%s\n",tempp);
    target_file = fopen(tempp,"w");
    if (target_file == NULL) {
        printf("Unable to open target file\n");
        exit(EXIT_FAILURE);
    }
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, source_file)) > 0) {
        if (fwrite(buffer, 1, bytes_read, target_file) != bytes_read) {
            printf("Error writing to target file\n");
            exit(EXIT_FAILURE);
        }
    }

    if (ferror(source_file)) {
        printf("Error reading from source file\n");
        exit(EXIT_FAILURE);
    }

    //printf("File copied successfully\n");

    fclose(source_file);
    fclose(target_file);
    remove(file_name_to_create);
		return EXIT_SUCCESS;
		//return 0;
	}
	else if (strcmp(argv[1], "-d") == 0)
	{
		FILE *a;
		//char filename_1[100],filename_2[100];
		file_directory=argv[2];
		//char esc[100];
		//sprintf(esc,"%s",file_directory);
		//printf("esc value: %s\n",esc);
		a = fopen(argv[2], "rb");
		if (a == NULL)
			fprintf(stderr, "Failed to complete decompression operation");
		else {
			untar(a, *argv);
			fclose(a);
		}
	}
	else if (strcmp(argv[1], "-l") == 0)
	{
		
        FILE *a;
                file_directory=argv[2];
                char temp[100];
                char slash[100]="/";
		sprintf(temp,"%s%s",file_directory);
		//printf("%s\n",temp);
		a = fopen(argv[2], "rb");
		if (a == NULL)
			fprintf(stderr, "Failed to complete list operation");
		else {
			listtar(argv[2], *argv);
			
			fclose(a);
			FILE *source_file, *target_file;
			file_name_to_create="tarStructure";
			file_directory=argv[2];
			remove_tar_extension(file_directory);
			char buffer[BUFFER_SIZE];
    size_t bytes_read;

    source_file = fopen(file_name_to_create, "r");
    if (source_file == NULL) {
        printf("Unable to open source file\n");
        exit(EXIT_FAILURE);
    }
    char tempp[100];
    char splash[100]="/";
    char path[MAX_PATH_LENGTH];
    char *last_slash;

    strcpy(path, argv[2]);

    last_slash = strrchr(path, '/');
    if (last_slash == NULL) {
        printf("Invalid file path\n");
        exit(EXIT_FAILURE);
    }

    *last_slash = '\0';

    //printf("Directory path: %s\n", path);
    sprintf(tempp,"%s%s%s",path,splash,file_name_to_create);
    //printf("%s\n",tempp);
    target_file = fopen(tempp,"w");
    if (target_file == NULL) {
        printf("Unable to open target file\n");
        exit(EXIT_FAILURE);
    }
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, source_file)) > 0) {
        if (fwrite(buffer, 1, bytes_read, target_file) != bytes_read) {
            printf("Error writing to target file\n");
            exit(EXIT_FAILURE);
        }
    }

    if (ferror(source_file)) {
        printf("Error reading from source file\n");
        exit(EXIT_FAILURE);
    }

    //printf("File copied successfully\n");

    fclose(source_file);
    fclose(target_file);
    remove(file_name_to_create);
			
        }
    }
    else
    {
        printf("Incorrect Usage");
    }
	return EXIT_FAILURE;
}
