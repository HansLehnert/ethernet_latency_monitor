#include "utils.h"

QString metricPrefix(double value) {
	int exp = 0;

	if (value != 0) {
		while (value > 1000) {
			exp++;
			value /= 1000;
		}

		while (value < 1) {
			exp--;
			value *= 1000;
		}
	}

	QString result = QString::number(value) + " ";
	switch (exp) {
	case 1:
		result += "k";
		break;
	case 2:
		result += "M";
		break;
	case 3:
		result += "G";
		break;
	case 4:
		result += "T";
		break;
	case 5:
		result += "P";
		break;
	case -1:
		result += "m";
		break;
	case -2:
		result += "u";
		break;
	case -3:
		result += "n";
		break;
	case -4:
		result += "p";
		break;
	}

	return result;
}
