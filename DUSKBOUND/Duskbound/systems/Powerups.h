#ifndef POWERUPS_H
#define POWERUPS_H

struct PowerupState {
	bool speedBoost;
	bool damageBoost;
	bool defenseBoost;
	bool bossLocationsRevealed;

	PowerupState() {
		speedBoost = false;
		damageBoost = false;
		defenseBoost = false;
		bossLocationsRevealed = false;
	}
};

extern PowerupState gPowerups;

#endif