#include <stdio.h>
#include <avr/io.h>
#include <math.h>
#include "avr.h"
#include "lcd.h"

struct datetime 
{
	int hour;
	int minute;
	int second;
	int subsecond;
} clock, alarm, timer;

struct Note
{
	int frequency;
	int duration;
};

struct Note TWINKLE_STAR[42] =
{
	//C		C		G		G		A		A		G
	{0, 1}, {0, 1}, {4, 1}, {4, 1}, {5, 1}, {5, 1}, {4, 0},
	//F       F        E      E      D       D       C
	{3, 1}, {3, 1}, {2, 1}, {2,1}, {1, 1}, {1, 1}, {0, 0},
	// G       G      F        F      E      E       D
	{4, 1}, {4, 1}, {3, 1}, {3, 1}, {2, 1}, {2, 1}, {1, 0},
	// G         G       F       F      E       E     D
	{4, 1}, {4, 1}, {3, 1}, {3, 1}, {2, 1}, {2, 1}, {1, 0},
	//C		C		G		G		A		A		G
	{0, 1}, {0, 1}, {4, 1}, {4, 1}, {5, 1}, {5, 1}, {4, 0},
	//F       F        E      E      D       D       C
	{3, 1}, {3, 1}, {2, 1}, {2,1}, {1, 1}, {1, 1}, {0, 0}
};

//							 C    D    E    F    G    A    B    C
int basicFrequencies[8] = {279, 313, 351, 372, 418, 468, 526, 557};
int chosenDuration[2] = {1, 2};
int duration = 150;
int currentNote = 0;

enum stateAlarm
{
	On, 
	Off
} alarmState;

enum stateTimer
{
	running, 
	stopped
} TIMER_STATE;

enum editStateAlarm
{
	alarmOff, 
	alarmOn
} editAlarm;

char keypad[17] =
{
	'1', '2', '3', 'A',
	'4', '5', '6', 'B',
	'7', '8', '9', 'C',
	'*', '0', '#', 'D'
};

void blinkTest(int keypadNum)
//This function is a test with the LED to blink accordingly with the number on the keypad
{
	DDRB = (DDRB | 1);
	char result = keypad[keypadNum - 1];
	if (result != 'A' && result != 'B' && result != 'C' && result != '*' && result != '#' && result != 'D')
	{
		int x = result - '0';
		for (int i = 0; i < x; ++i)
		{
			SET_BIT(PORTB, 0);
			avr_wait(200);
			CLR_BIT(PORTB, 0);
			avr_wait(200);
		}
	}
}

int is_pressed(int row, int col)
//This function allows you to press the correct keypad number
{
	//Set all pins of C to n/c
	DDRC = 0;
	PORTC = 0;
	//Set Port C's #C GPIO to Strong 0
	//SET PORT C's #R GPIO to Weak 1
	SET_BIT(DDRC, row);
	SET_BIT(PORTC, col + 4);
	avr_wait(1);
	if (GET_BIT(PINC, col + 4))
	{
		return 0;
	}
	return 1;
}

int get_key()
//This function gets you the key of the row and column
{
	int r, c;
	for (r = 0; r < 4; ++r)
	{
		for (c = 0; c < 4; ++c)
		{
			if (is_pressed(r, c))
			{
				return (r * 4) + c + 1;
			}
		}
	}
	return 0;
}

void play_note(int freq, int dura)
{
	float mytime = 18000.0;
	int totalTime = ceil((mytime / freq));

	int timeHigh = totalTime / 2;
	int timeLow = totalTime - timeHigh;

	for (int i = 0; i < (dura * 100 / totalTime); ++i)
	{
		SET_BIT(PORTB, 0);
		avr_wait(timeHigh);
		CLR_BIT(PORTB, 0);
		avr_wait(timeLow);
	}
}

void incrementTime()
//This increments the clock and adjusts the time accordingly.
{
	if (clock.subsecond >= 10)
	{
		clock.subsecond = 0;
		clock.second++;
	}
	if (clock.second >= 60)
	{
		clock.second = 0;
		clock.minute++;
	}
	if (clock.minute >= 60)
	{
		clock.minute = 0;
		clock.hour++;
	}
	if (clock.hour >= 24)
	{
		clock.hour = 0;
		clock.day++;
	}
	if (clock.day >= daysEachMonth[clock.month])
	{
		//Account for leap year
		if (clock.month == 1 && clock.year % 4 == 0)
		{
			if (clock.year % 100 != 0 || clock.year % 400 == 0)
			{
				if (clock.day > 28)
				{
					clock.day = 0;
					clock.month++;
				}
				
				else if (clock.day == 27)
				{
					return;
				}
			}
		}
		else
		{
			clock.day = 0;
			clock.month++;
		}
	}
	if (clock.month >= 12)
	{
		clock.month = 0;
		clock.year++;
	}
}

void adjustTime(int num)
{
	if (changeEditPosition == 0)
	{
		if (clock.month + num >= 0)
		{
			clock.month += num;
		}
	}
	else if (changeEditPosition == 1)
	{
		if (clock.day + num >= 0)
		{
			clock.day += num;
		}
	}
	else if (changeEditPosition == 2)
	{
		if (clock.year + num >= 0)
		{
			clock.year += num;
		}
	}
	else if (changeEditPosition == 3)
	{
		if (clock.hour + num >= 0)
		{
			clock.hour += num;
		}
	}
	else if (changeEditPosition == 4)
	{
		if (clock.minute + num >= 0)
		{
			clock.minute += num;
		}
	}
	else if (changeEditPosition == 5)
	{
		if (clock.second + num >= 0)
		{
			clock.second += num;
		}
	}
}

void printToLED()
{
	//Clear LED
	lcd_clr();
	
	//This is top row of the LED
	lcd_pos(0,0);
	char buf[20];
	char positionNum = 0;
	char P = "";
	char O = "";
	char S = "";
	
	if (editTime)
	{
		P = "P";
		O = "O";
		S = "S";
		positionNum = changeEditPosition + 49;
	}
	sprintf(buf, "%02d/%02d/%04d  %s%s%s%c", clock.month + 1, clock.day + 1, clock.year, P, O, S, positionNum);
	lcd_puts2(buf);

	//This is bottom row
	lcd_pos(1,0);
	int hour = clock.hour;
	if (!(usingMilitary))
	{
		char meridiam = "";
		int meridiamHour = hour;
		if (hour % 12 != 0)
		{
			meridiamHour = meridiamHour % 12;
		}
		else
		{
			meridiamHour = 12;
		}
		if (!(hour >= 12))
		{
			meridiam = "AM";
		}
		else
		{
			meridiam = "PM";
		}
		sprintf(buf, "%02d:%02d:%02d %s", meridiamHour, clock.minute, clock.second, meridiam);
	}
	else
	{
		sprintf(buf, "%02d:%02d:%02d", hour, clock.minute, clock.second);
	}
	lcd_puts2(buf);
}

void clearTime()
{
	settingTime = 0;
	timeSet = 0;
	timeSetPosition = 0;
}

void setTime()
{
	//No changes needed
	else if (changeEditPosition == 3)
	{
		clock.hour = timeSet;
	}
	else if (changeEditPosition == 4)
	{
		clock.minute = timeSet;
	}
	else if (changeEditPosition == 5)
	{
		clock.second = timeSet;
	}
	//Changes needed
	else if (changeEditPosition == 0)
	{
		if (timeSet > 0)
		{
			clock.month = timeSet - 1;
		}
	}
	else if (changeEditPosition == 1)
	{
		if (timeSet > 0)
		{
			clock.day = timeSet - 1;
		}
	}
}

void ringAlarm()
{
	currentNote = 0;
	while (currentNote < 42)
	{
		struct Note SpecificNote = TWINKLE_STAR[currentNote];
		play_note(basicFrequencies[SpecificNote.frequency], (duration/chosenDuration[SpecificNote.duration]));
		++currentNote;
	}
}

int main(void)
{
	//Initialize these once
	avr_init();
	lcd_init();
	DDRB = 0x01;
	
	//Clock increments every loop
	while (1)
	{
		if (clock.hour == alarm.hour && clock.minute == alarm.minute && clock.second == alarm.minute)
		{
			stateAlarm = On;
			ringAlarm();
			stateAlarm = Off;
			currentNote = 0;
		}
		
		++clock.subsecond;
		int key = (get_key() - 1);
		if (key == -1) {}

		else if (keypad[key] == 'C')
		{
			editTime = (editTime + 1) % 2;
			clearTime();
		}
		
		else if (keypad[key] == 'D')
		{
			usingMilitary = (usingMilitary + 1) % 2;
		}
		
		else if (editTime)
		{
			if (keypad[key] == 'A')
			{
				adjustTime(1);
			}
			else if (keypad[key] == 'B')
			{
				adjustTime(-1);
			}
			else if (keypad[key] == '*')
			{
				if (changeEditPosition > 0)
				{
					changeEditPosition--;
				}
			}
			else if (keypad[key] == '#')
			{
				if (changeEditPosition < 5)
				{
					++changeEditPosition;
				}
			}
			else
			{
				int num = keypad[key] - 48;
				++timeSetPosition;
				settingTime = 1;
				timeSet = (timeSet * 10) + num;
				if (((timeSetPosition == 4) && (changeEditPosition == 2)) || ((timeSetPosition == 2) && (changeEditPosition != 2)))
				{
					setTime();
					clearTime();
				}
			}
		}
		printToLED();
		incrementTime();
		avr_wait(100);
	}
}