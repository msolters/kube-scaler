#include <stdio.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <lcd.h>
#include <sys/time.h>
#include <stdbool.h>

#define DEPLOY_LENGTH 256
#define LCD_WIDTH 16
#define RIGHT_BTN 21
#define LEFT_BTN 22
const unsigned char context[] = "sandbox";

double lastRender;
double lastDeployChange;
int deployNameOffset;
int fd; // handle for LCD display
int numDeploys;

long long current_timestamp() {
  struct timeval te;
  gettimeofday(&te, NULL); // get current time
  long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // calculate milliseconds
  // printf("milliseconds: %lld\n", milliseconds);
  return milliseconds;
}

void resetDeploy() {
  deployNameOffset = 0;
  lastRender = current_timestamp();
}

void getDeploys()
{
  // Write all deploys to file
  char kubeCMD[256];
  sprintf(kubeCMD, "kubectl --context %s get deploy -o wide | tail -n +2 > /tmp/kube-deploys", context);
  printf("%s\n", kubeCMD);
  system(kubeCMD);

  // Count number of deploys in file
  int lines = 0;
  FILE *fp = fopen("/tmp/kube-deploys", "r");
  char ch;
  while(!feof(fp)) {
    ch = fgetc(fp);
    if(ch == '\n') lines++;
  }
  fclose(fp);
  numDeploys = lines;
}

int selected_deploy = 0;
typedef struct deploy {
  int replicas;
  char name[DEPLOY_LENGTH];
} deploy_t;
deploy_t current_deploy;

void setDeploy(int replicas, char *name) {
  deploy_t *d = &current_deploy;
  if(name != NULL) strcpy(d->name, name);
  if(replicas != NULL) d->replicas = replicas;
  return 0;
}

void retrieveDeploy()
{
  char retrieve_deploy_cmd[50];
  sprintf(retrieve_deploy_cmd, "sed -n '%up' /tmp/kube-deploys | awk '{ print $1 \"\\n\" $2 }'", selected_deploy+1);

  FILE *deploy_data = popen(retrieve_deploy_cmd, "r");
  if( deploy_data == NULL ) {
    printf("Failed to retrieve kubernetes deploy at index %u.\n", selected_deploy);
    exit(1);
  }

  char deploy[DEPLOY_LENGTH];
  int line = 0;
  while( fgets(deploy, sizeof(deploy)-1, deploy_data) != NULL ) {
    //printf("%s\n", deploy);
    switch( line++ ) {
        case 0:
            deploy[strlen(deploy)-1] = '\0';
            setDeploy(NULL, deploy);
            break;
        case 1:
            setDeploy(atoi(deploy), NULL);
            break;
        default:
            break;
    }
  }

  pclose(deploy_data);
}

void printDeploy(deploy_t *d) {
  printf("Name: %s\n", d->name);
  printf("Replicas: %u\n", d->replicas);
  return;
}

void initButtons() {
  pinMode(LEFT_BTN, INPUT);
  pinMode(RIGHT_BTN, INPUT);
  pullUpDnControl(LEFT_BTN, PUD_UP);
  pullUpDnControl(RIGHT_BTN, PUD_UP);
}

void updateSelectedDeployment() {
  retrieveDeploy();
  resetDeploy();
}

void checkButtons() {
  bool deploy_changed = false;
  int deploy_change = 0;
  if( digitalRead(LEFT_BTN) == 0 ) {
    deploy_change = -1;
    deploy_changed = true;
  }
  if( digitalRead(RIGHT_BTN) == 0 ) {
    deploy_change = 1;
    deploy_changed = true;
  }
  double now = current_timestamp();
  if( deploy_changed && (now-lastDeployChange) > 750 ) {
    if( selected_deploy == 0 && deploy_change < 0 ) deploy_change = 0;
    if( selected_deploy == (numDeploys-1) && deploy_change > 0 ) deploy_change = 0;
    if( deploy_change == 0 ) return;
    selected_deploy += deploy_change;
    updateSelectedDeployment();
    lastDeployChange = now;
  }
}

void renderDeploy() {
  double now = current_timestamp();
  double elapsed = now - lastRender;
  if( elapsed < 300 ) return;
  lastRender = now;

  deploy_t *d = &current_deploy;

  // Deploy Name (Scrolling)
  char *name;
  int deployNameLength = strlen(d->name) + LCD_WIDTH;
  name = malloc( deployNameLength * sizeof(char) );
  name[0] = '\0';
  strcat(name, d->name);
  for( int k=0; k<LCD_WIDTH; k++ ) {
    strcat(name, " ");
  }
  int lcdPosX = 0;
  char deployName[LCD_WIDTH+1];
  deployName[LCD_WIDTH] = '\0';
  for( lcdPosX = 0; lcdPosX < LCD_WIDTH && lcdPosX < deployNameLength; lcdPosX++ ) {
    deployName[ lcdPosX ] = name[(deployNameOffset+lcdPosX) % deployNameLength];
  }
  free(name);
  deployNameOffset++;

  lcdPosition(fd, 0, 0);
  lcdPuts(fd, deployName);

  // Deploy Replicas
  lcdPosition(fd, 0, 1);
  char deployReplicas[LCD_WIDTH+1];
  sprintf(deployReplicas, "%u Replicas", d->replicas);
  lcdPuts(fd, deployReplicas);

  return;
}

void initLCD() {
	fd = lcdInit(2,16,4, 2,3, 6,5,4,1,0,0,0,0); //see /usr/local/include/lcd.h
	printf("%d", fd);
	if (fd == -1){
		printf("lcdInit 1 failed\n") ;
		return 1;
	}
	sleep(1);

	lcdClear(fd);
}

int main(void)
{
    getDeploys();
    retrieveDeploy();

	if (wiringPiSetup() == -1){
		exit(1);
	}

    initLCD();
    initButtons();

    resetDeploy();
    while( 1 ) {
      renderDeploy();
      checkButtons();
    }
	sleep(1);

    return 0;
}

