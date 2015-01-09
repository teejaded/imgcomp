#include <stdio.h>
#include <ctype.h>		// to declare isupper(), tolower() 
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#ifdef _WIN32
    #include "readdir.h"
    #define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
    #define strdup(a) _strdup(a) 
#else
#endif

#include "imgcomp.h"

static const char * progname;  // program name for error messages
static char * outfilename;	   // for -outfile switch
static char * DoDirName = NULL;
static int ScaleDenom;


void usage (void)// complain about bad command line 
{
  fprintf(stderr, "usage: %s [switches] ", progname);
  fprintf(stderr, "inputfile outputfile\n");

  fprintf(stderr, "Switches (names may be abbreviated):\n");
  fprintf(stderr, "  -grayscale     Force grayscale output\n");
#ifdef IDCT_SCALING_SUPPORTED
  fprintf(stderr, "  -scale N     Scale output image by fraction 1/N, eg, 1/8.  Default 1/4\n");
#endif
  fprintf(stderr, "Switches for advanced users:\n");
  fprintf(stderr, "  -outfile name  Specify name for output file\n");
  fprintf(stderr, "  -verbose  or  -debug   Emit debug output\n");
  exit(-1);
}

//----------------------------------------------------------------------
// Case-insensitive matching of possibly-abbreviated keyword switches.
// keyword is the constant keyword (must be lower case already),
// minchars is length of minimum legal abbreviation.
//----------------------------------------------------------------------
static int keymatch (char * arg, const char * keyword, int minchars)
{
    int ca, ck, nmatched = 0;

    while ((ca = *arg++) != '\0') {
        if ((ck = *keyword++) == '\0') return 0;  // arg longer than keyword, no good */
        if (isupper(ca))  ca = tolower(ca);	 // force arg to lcase (assume ck is already)
        if (ca != ck)  return 0;             // no good 
        nmatched++;			                 // count matched characters 
    }
    // reached end of argument; fail if it's too short for unique abbrev 
    if (nmatched < minchars) return 0;
    return 1;
}


//-----------------------------------------------------------------------------------
// Parse command line switches
// Returns argv[] index of first file-name argument (== argc if none).
//-----------------------------------------------------------------------------------
static int parse_switches (int argc, char **argv, int last_file_arg_seen, int for_real)
{
    int argn;
    char * arg;

    ScaleDenom = 4;
    DoDirName = NULL;

    // Scan command line options, adjust parameters

    for (argn = 1; argn < argc; argn++) {
        printf("argn = %d\n",argn);
        arg = argv[argn];
        if (*arg != '-') {
            // Not a switch, must be a file name argument 
            if (argn <= last_file_arg_seen) {
                outfilename = NULL;	
                continue;		/* ignore this name if previously processed */
            }
            break;			/* else done parsing switches */
        }
        arg++;			/* advance past switch marker character */

        if (keymatch(arg, "debug", 1) || keymatch(arg, "verbose", 1)) {
            // Enable debug printouts.  Specify more than once for more detail.
            //cinfo->err->trace_level++;

        } else if (keymatch(arg, "grayscale", 2) || keymatch(arg, "greyscale",2)) {
            // Force monochrome output.
            //cinfo->out_color_space = JCS_GRAYSCALE;
        } else if (keymatch(arg, "outfile", 4)) {
            // Set output file name.
            if (++argn >= argc)	// advance to next argument
	           usage();
            outfilename = argv[argn];	// save it away for later use

        } else if (keymatch(arg, "scale", 1)) {
            // Scale the output image by a fraction M/N.
            if (++argn >= argc)	// advance to next argument
                 usage();
            if (sscanf(argv[argn], "%d", &ScaleDenom) != 1)
               usage();

        } else if (keymatch(arg, "dodir", 1)) {
            // Scale the output image by a fraction M/N. */
            if (++argn >= argc)	// advance to next argument
                 usage();
            DoDirName = argv[argn];
        } else {
            usage();	   // bogus switch
        }
    }

    return argn;		   // return index of next arg (file name)
}


//-----------------------------------------------------------------------------------
// Compare file names to sort directory.
//-----------------------------------------------------------------------------------
static int fncmpfunc (const void * a, const void * b)
{
    printf("compare %s %s\n",*(char **)a,*(char **)b);
    return strcmp(*(char **)a, *(char **)b);
}

//-----------------------------------------------------------------------------------
// Process a whole directory.
//-----------------------------------------------------------------------------------
int DoDirectory(char * Directory)
{
    char ** FileNames;
    int NumFileNames;
    int NumAllocated;
    DIR * dirp;
    char catpath[500];
    int pathlen;

    NumAllocated = 5;
    FileNames = malloc(sizeof (char *) * NumAllocated);

    NumFileNames = 0;
  
    dirp = opendir(Directory);
    if (dirp == NULL){
        fprintf(stderr, "could not open dir\n");
        return -1;
    }
    strncpy(catpath, Directory,300);
    pathlen = strlen(catpath);
    if (catpath[pathlen-1] != '/' && catpath[pathlen-1] != '\\'){
        catpath[pathlen] = '/';
        pathlen += 1;
    }

    for (;;){
        struct dirent * dp;
        struct stat buf;
        int l;
        errno = 0;
        dp = readdir(dirp);
        if (dp == NULL) break;
        printf("name: %s %d %d\n",dp->d_name, dp->d_off, dp->d_reclen);

        // Check that it's a regular file.
        strncpy(catpath+pathlen, dp->d_name,100);
        stat(catpath, &buf);
        if (!S_ISREG(buf.st_mode)) continue; // not a file.

        // Check that name ends in ".jpg", ".jpeg", or ".JPG", etc...
        l = strlen(dp->d_name);
        if (l < 5) continue;
        if (dp->d_name[l-1] != 'g' && dp->d_name[l-1] != 'G') continue;
        if (dp->d_name[l-2] == 'e' || dp->d_name[l-2] == 'E') l-= 1;
        if (dp->d_name[l-2] != 'p' && dp->d_name[l-2] != 'P') continue;
        if (dp->d_name[l-3] != 'j' && dp->d_name[l-3] != 'J') continue;
        if (dp->d_name[l-4] != '.') continue;
        printf("use: %s %d %d\n",dp->d_name, dp->d_off, dp->d_reclen);

        if (NumFileNames >= NumAllocated){
            printf("realloc\n");
            NumAllocated *= 2;
            FileNames = realloc(FileNames, sizeof (char *) * NumAllocated);
        }

        FileNames[NumFileNames++] = strdup(dp->d_name);
    }
    closedir(dirp);

    // Now sort the names (could be in random order)
    qsort(FileNames, NumFileNames, sizeof(char **), fncmpfunc);

    {
        int a;
        for (a=0;a<NumFileNames;a++){
            printf("sorted: %s\n",FileNames[a]);
        }
    }

    return -1;
}

//-----------------------------------------------------------------------------------
// The main program.
//-----------------------------------------------------------------------------------
int main (int argc, char **argv)
{
    int file_index;
    progname = argv[0];

    // Scan command line to find file names.
    file_index = parse_switches(argc, argv, 0, 0);
    
    if (DoDirName){
        return DoDirectory(DoDirName);
    }


    if (argc-file_index == 2){
        MemImage_t *pic1, *pic2;
        
        printf("load %s\n",argv[file_index]);
        pic1 = LoadJPEG(argv[file_index], ScaleDenom, 0);
        printf("\nload %s\n",argv[file_index+1]);
        pic2 = LoadJPEG(argv[file_index+1], ScaleDenom, 0);
        if (pic1 && pic2){
            ComparePix(pic1, pic2, "diff.ppm");
        }
        free(pic1);
        free(pic2);
    }else{
        int a;
        MemImage_t * pic;

        for (a=file_index;a<argc;a++){

            printf("input file %s\n",argv[a]);
            // Load file into memory.

            pic = LoadJPEG(argv[a], 4, 0);
            if (pic) WritePpmFile("out.ppm",pic);
        }
    }

    return 0;
}