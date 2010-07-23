/*
Copyright 2010, Adrien Destugues <pulkomandy@pulkomandy.ath.cx>
Distributed under the terms of the MIT License.
*/


#include <TimeZone.h>

#include <String.h>

#include <unicode/timezone.h>
#include <ICUWrapper.h>


BTimeZone::BTimeZone(const char* zoneCode)
{
	fICUTimeZone = TimeZone::createTimeZone(zoneCode);
}


BTimeZone::~BTimeZone()
{
	delete fICUTimeZone;
}


void
BTimeZone::Name(BString& name)
{
	UnicodeString unicodeName;
	fICUTimeZone->getDisplayName(unicodeName);

	BStringByteSink converter(&name);
	unicodeName.toUTF8(converter);
}


void
BTimeZone::Code(BString& code)
{
	UnicodeString unicodeName;
	fICUTimeZone->getID(unicodeName);

	BStringByteSink converter(&code);
	unicodeName.toUTF8(converter);
}


int
BTimeZone::OffsetFromGMT()
{
	int32_t rawOffset;
	int32_t dstOffset;
	time_t now;
	UErrorCode error = U_ZERO_ERROR;
	fICUTimeZone->getOffset(time(&now) * 1000, FALSE, rawOffset, dstOffset
		, error);
	if (error != U_ZERO_ERROR)
		return 0;
	else
		return rawOffset + dstOffset;
}
