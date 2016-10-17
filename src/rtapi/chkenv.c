
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <string.h>

int main(int argc, char *argv[])
{
struct passwd *pw;
char path[80];
char *flavour = NULL;
char *id = NULL;
char *build_sys = NULL;

	pw = getpwuid(getuid());
	strcpy(path, pw->pw_dir);
	strcat(path, "/.bashrc");
	
	FILE *fp;
	fp = fopen((const char *) path, "a+");
	flavour = getenv ("FLAVOR");
	    // check also if env var has been set, but contains nothing except maybe CR
	    // occurs if .bashrc has `export FLAVOR=$(flavor)` say, but flavor binary not built yet
	if (flavour == NULL || (flavour != NULL && strlen(flavour) < 2))
		fprintf(fp, "\nexport FLAVOR=$(echo $(flavor))\n");
		
	id = getenv ("FLAVOR_ID");
	if (id == NULL || (flavour != NULL && strlen(flavour) < 2))
		fprintf(fp, "\nexport FLAVOR_ID=$(echo $(flavor -d))\n");
		
	build_sys = getenv ("BUILD_SYS");
	if (build_sys == NULL || (flavour != NULL && strlen(flavour) < 2))
		fprintf(fp, "\nexport BUILD_SYS=$(echo $(flavor -b))\n");
	
	fclose(fp);
	
	if(flavour == NULL | id == NULL | build_sys == NULL)
		{
		printf("Shell environment was incomplete - update bash shell\n");
		return (1);
		}
	else
		{
		printf("Shell environment OK\n");
		return(0);
		}
}
