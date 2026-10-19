#include "player.h"
#include <string.h>

Player *InitPlayer(Player *p, int id, char username[50], int plan, int gems, int power){
 p->id = id;
 strcpy(p->username, username);
 p->plan = plan;
 p->gems = gems;
 p->power = power;
 return p;  
}
