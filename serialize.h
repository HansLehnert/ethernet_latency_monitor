#ifndef SERIALIZE_H
#define SERIALIZE_H

#include <QVector>

template <class T>
inline QVector<uchar> serialize(T value) {
	QVector<uchar> result;
	result.resize(sizeof(T));

	for (int i = 0; i < sizeof(T); i++) {
		result[i] = (uchar)((value >> (8 * (sizeof(T) - 1 - i))) & 0xFF);
	}

	return result;
}

template <class T>
inline T deserialize(QVector<uchar> byte_array) {
	T result = 0;

	for (int i = 0; i < sizeof(T); i++) {
		result <<= 8;
		result |= byte_array[i];
	}

	return result;
}

#endif // SERIALIZE_H
