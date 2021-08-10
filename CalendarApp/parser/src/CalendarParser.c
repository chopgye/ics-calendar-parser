#include "CalendarParser.h"
#include "LinkedListAPI.h"
#include <ctype.h>
#include <strings.h>
#include <limits.h>
#define DEBUG 0
/*
    Name: Dorjee Tenzin
    ID: 0980180
    CIS*2750 - Assignment 2
*/
/* Function definitions */
char ** readFile (char * , int *, ICalErrorCode *);
char ** findProperty(char ** file, int beginIndex, int endIndex, char * propertyName, bool once, int * count, ICalErrorCode *);
char * getToken ( char * entireFile, int * index, ICalErrorCode *);
ICalErrorCode checkFormatting( char ** entireFile, int numLines, int ignoreCalendar);
char ** getAllPropertyNames (char ** file, int beginIndex, int endIndex, int * numElements, int * errors);
int validateStamp ( char * check );
Alarm * createAlarm(char ** file, int beginIndex, int endIndex);
char ** unfold (char ** file, int numLines, int * setLines );
char * getProp (const char * prop) ;
char * escape (char * string ) ;
int validLength(const char * string, int length );
char* eventListToJSONWrapper(const Calendar* cal);
char* alarmToJSON(const Alarm * alarm);
char* alarmListToJSON(const List* al);
char* alarmListToJSONWrapper(const Calendar * cal, int eventNumber);
char * propertyToJSON(const Property * prop);
char * propertyListToJSON(const List * pl);
void JSONtoEventWrapper(Calendar * cal, char * json);


const char eventprops[25][30] = { "CLASS1", "CREATED1", "DESCRIPTION1", "GEO1", "LAST-MODIFIED1", "LOCATION1", "ORGANIZER1", "PRIORITY1",
                            "SEQUENCE1", "STATUS1", "SUMMARY1", "TRANSP1", "URL1", "RECURRENCE-ID1", "RRULE", "ATTACH", "ATTENDEE",
                            "CATEGORIES", "COMMENT", "CONTACT", "EXDATE", "REQUEST-STATUS", "RELATED-TO", "RESOURCES", "RDATE"};

char* printError(ICalErrorCode err) {
    char * result = (char *) calloc ( 1, 500);
    switch ( err ) {
        case OK:
            strcpy(result, "OK");
        break;
        case INV_FILE:
            strcpy(result, "INV_FILE");
        break;
        case INV_CAL:
            strcpy(result, "INV_CAL");
        break;
        case INV_VER:
            strcpy(result, "INV_VER");
        break;
        case DUP_VER:
            strcpy(result, "DUP_VER");
        break;
        case INV_PRODID:
            strcpy(result, "INV_PRODID");
        break;
        case DUP_PRODID:
            strcpy(result, "DUP_PRODID");
        break;
        case INV_EVENT:
            strcpy(result, "INV_EVENT");
        break;
        case INV_DT:
            strcpy(result, "INV_DT");
        break;
        case INV_ALARM:
            strcpy(result, "INV_ALARM");
        break;
        case WRITE_ERROR:
            strcpy(result, "WRITE_ERROR");
        break;
        default: strcpy(result, "OTHER_ERROR");
    }
    return result;
}

ICalErrorCode createCalendar(char* fileName, Calendar** obj) {
    int numLines = 0, correctVersion = 1, fnLen = 0;
    /* If the filename is null, or the string is empty */
    if (fileName == NULL || strcasecmp(fileName, "") == 0) return INV_FILE;
    /* If the calendar object doesn't point to anything */
    /* createCalendar("file.ics", NULL) is invalid because it attempts to de-reference NULL */
    /* I stand by that ^ */
    if (obj == NULL) return OTHER_ERROR;

    /* Check file extension */
    fnLen = strlen(fileName);
    if (toupper(fileName[fnLen - 1]) != 'S' || toupper(fileName[fnLen - 2]) != 'C' || toupper(fileName[fnLen - 3]) != 'I' || fileName[fnLen - 4] != '.') {
        return INV_FILE;
    }

    /* Attempt to open the file */
    FILE * calendarFile = fopen(fileName, "r");

    /* See if it's NULL */
    if (calendarFile == NULL) return INV_FILE;

    //At this point we already know the file exists, and is valid.
    fclose( calendarFile );
    ICalErrorCode __error__ = OK;
    /* Assuming everyt1hing has worked */
    char ** entire_file = readFile ( fileName, &numLines, &__error__);

    if (entire_file == NULL) return __error__;
    /* Does not begin or end properly */

    int temp = numLines;
    numLines = 0;

    char ** file = unfold(entire_file, temp, &numLines);

    /* Free up the old format */
    for (int kk = 0; kk < temp; kk++) free(entire_file[kk]);
    free(entire_file);

    /* Set it back up */
    entire_file = file;

    if (strcasecmp(entire_file[0], "BEGIN:VCALENDAR\r\n") || strcasecmp(entire_file[numLines - 1], "END:VCALENDAR\r\n")) {
        #if DEBUG
            printf("The beginning line is: %s\nThe end is %s", entire_file[0], entire_file[numLines - 1]);
            printf("The file doesn't follow the correct start/end protocol. Exiting.\n");
        #endif
        for (int i = 0; i < numLines; i++) free(entire_file[i]);
        free(entire_file);
        /* Free each array, and then free the whole thing itself */
        return INV_CAL;
    }

    /* Validates the formatting of the file */
    ICalErrorCode formatCheck = checkFormatting(entire_file, numLines, 0);
    if ( formatCheck != OK ) {
        #if DEBUG
            printf("It seems that the layout of the file events & alarms is incorrect\n");
        #endif
        for (int i = 0; i < numLines; i++) free(entire_file[i]);
        free(entire_file);
        return formatCheck;
    }

    __error__ = OK;
    *obj = (Calendar *) calloc ( 1, sizeof ( Calendar ) );
    /* Raw values */
    int vcount = 0, pcount = 0;
    char ** version = findProperty(entire_file, 0, numLines, "VERSION:", true, &vcount, &__error__);

    /* Trim any whitespace */
    /* Check to make sure that version is a number */
    if (version == NULL || strlen(version[0]) == 0) {
        #if DEBUG
            printf("Error: Version is NULL\n");
        #endif
        /* Free memory */
        if (version) {
            free(version[0]);
            free(version);
        }
        for (int i = 0; i < numLines; i++) free(entire_file[i]);
        free(entire_file);
        deleteCalendar(*obj);

        if (__error__ == OTHER_ERROR) return DUP_VER;

        return version == NULL ? INV_CAL : INV_VER;
    }

    char ** prodId = findProperty(entire_file, 0, numLines, "PRODID:", true, &pcount, &__error__);
    /* Trim whitespace
    trim(&version[0]);
    */
    /* Questionable */
    if (prodId == NULL || version == NULL || strlen(version[0]) == 0 || strlen(prodId[0]) == 0) {
        #if DEBUG
            printf("Error: Either version is empty after trimming, or prodId is empty.\n");
        #endif
        if (prodId) { free(prodId[0]); free(prodId); }
        if (version) { free(version[0]); free(version); }
        for (int i = 0; i < numLines; i++) free(entire_file[i]);
        free(entire_file);
        deleteCalendar(*obj);
        *obj = NULL;
        if (__error__ == OTHER_ERROR) return DUP_PRODID;
        return prodId == NULL ? INV_CAL : INV_PRODID;
    }
    /* It's not a number, cannot be represented by a float */
    for (int i = 0; i < strlen(version[0]); i++) {
        if ( (version[0][i] >= '0' && version[0][i] <= '9') || version[0][i] == '.'  || version[0][i] == '\r' || version[0][i] == '\n') continue;
        correctVersion = 0;
    }
    /* Basically, the version is not a number */
    if (!correctVersion) {
        #if DEBUG
            printf("Error: Version is not a floating point\n");
        #endif
        if (prodId) { free(prodId[0]); free(prodId); }
        if (version) { free(version[0]); free(version); }
        for (int i = 0; i < numLines; i++) free(entire_file[i]);
        free(entire_file);
        deleteCalendar(*obj);
        return INV_VER;
    }
    /* Write the version & production id */
    (*obj)->version = (float) atof(version[0]);
    strcpy((*obj)->prodID, prodId[0]);

    /* Free */
    if (prodId) { free(prodId[0]); free(prodId); }
    if (version) { free(version[0]); free(version); }

    /* After we're done extracting the version and prodId, we can begin extracting all other calendar properties */
    int N = 0, errors = 0;
    char ** calendarProperties = getAllPropertyNames(entire_file, 0, numLines, &N, &errors);
    /* If any errors occured */

    if (errors || !N) {
        for (int x = 0; x < N; x++) free(calendarProperties[x]);
        free(calendarProperties);
        for (int i = 0; i < numLines; i++) free(entire_file[i]);
        free(entire_file);
        deleteCalendar(*obj);
        return INV_CAL;
    }

    (*obj)->properties = initializeList(printProperty, deleteProperty, compareProperties);

    /* Grab all calendar properties */
    for (int x = 0; x < N; x++) {
        /* We already checked those guys */
        if (strcasecmp(calendarProperties[x], "PRODID:") && strcasecmp(calendarProperties[x], "VERSION:") && strcasecmp(calendarProperties[x], "PRODID;") && strcasecmp(calendarProperties[x], "VERSION;")) {
            int count = 0;
            /* All of these properties can have more than one value */
            __error__ = OK;
            char ** value = findProperty(entire_file, 0, numLines, calendarProperties[x], false, &count, &__error__);
            /* Then there is an error */
            if (value == NULL || strlen(value[0]) == 0) {
                for (int x = 0; x < N; x++) free(calendarProperties[x]);
                free(calendarProperties);
                if (value) {
                    for (int ii = 0; ii < count; ii++) free(value[ii]);
                    free(value);
                }
                for (int i = 0; i < numLines; i++) free(entire_file[i]);
                free(entire_file);
                deleteCalendar(*obj);
                return INV_CAL;
            }
            for (int c = 0; c < count; c++) {
                Property * prop = NULL;
                prop = (Property *) calloc ( 1, sizeof (Property) + sizeof(char) * (1 + strlen(value[c])));
                int j = 0;
                /* We don't want to include the delimeter */
                for ( ; j < strlen(calendarProperties[x]) - 1; j++)
                    prop->propName[j] = calendarProperties[x][j];
                prop->propName[j] = '\0';
                strcpy(prop->propDescr, value[c]);
                insertBack((*obj)->properties, prop);
            }
            /* Free entire array */
            for (int c = 0; c < count; c++)
                free(value[c]);
            free(value);
        }
        /* Free the element */
        free(calendarProperties[x]);
    }
    /* Free the 2d array */
    free(calendarProperties);

    /* Allocate some memory for the events */
    (*obj)->events = initializeList(printEvent, deleteEvent, compareEvents);
    int eventCount = 0;

    /* Start tracking the events */
    for (int beginLine = 1; beginLine < numLines; ) {
        if (!strcasecmp(entire_file[beginLine], "BEGIN:VEVENT\r\n")) {
            #if DEBUG
                printf("NEW EVENT\n");
            #endif
            int endLine = beginLine + 1;
            /* Track the end */
            for ( ; endLine < numLines; endLine++) {
                if (!strcasecmp(entire_file[endLine], "END:VEVENT\r\n")) {
                    break;
                }
            }

            /* Required property for all events */
            char ** UID = NULL;
            int uidc = 0;
            __error__ = OK;
            UID = findProperty(entire_file, beginLine, numLines, "UID:", true, &uidc, &__error__);

            if (UID == NULL || !strlen(UID[0])) {
                if (UID) { free(UID[0]); free(UID); } /* empty string or something */
                for (int i = 0; i < numLines; i++) free(entire_file[i]);
                free(entire_file);
                #if DEBUG
                    printf("Error! Some event is missing a UID\n");
                #endif
                deleteCalendar(*obj);
                return INV_EVENT;
            }

            char ** DTSTAMP = NULL;
            int dtc = 0;
            __error__ = OK;
            DTSTAMP = findProperty(entire_file, beginLine, numLines, "DTSTAMP:", true, &dtc, &__error__);

            if (DTSTAMP == NULL || !strlen(DTSTAMP[0])) {
                if (DTSTAMP) {
                    free(DTSTAMP[0]); free(DTSTAMP);
                    if (UID) { free(UID[0]); free(UID); }
                    for (int i = 0; i < numLines; i++) free(entire_file[i]);
                    free(entire_file);
                    #if DEBUG
                        printf("Error! The dtstamp property was not found!\n");
                    #endif
                    deleteCalendar(*obj);
                    *obj = NULL;
                    return __error__ == OTHER_ERROR ? INV_EVENT : INV_DT;
                }
                __error__ = OK;
                DTSTAMP = findProperty(entire_file, beginLine, numLines, "DTSTAMP;", true, &dtc, &__error__);
                if (DTSTAMP) {
                    free(DTSTAMP[0]); free(DTSTAMP);
                    if (UID) { free(UID[0]); free(UID); }
                    for (int i = 0; i < numLines; i++) free(entire_file[i]);
                    free(entire_file);
                    #if DEBUG
                        printf("Error! The dtstamp property was not found!\n");
                    #endif
                    deleteCalendar(*obj);
                    *obj = NULL;
                    return __error__ == OTHER_ERROR ? INV_EVENT : INV_DT;
                }
                if (UID) { free(UID[0]); free(UID); }
                for (int i = 0; i < numLines; i++) free(entire_file[i]);
                free(entire_file);
                #if DEBUG
                    printf("Error! The dtstamp property was not found!\n");
                #endif
                deleteCalendar(*obj);
                *obj = NULL;
                return INV_EVENT;
            }
            /* If it was found, but it's invalid */
            if (!validateStamp(DTSTAMP[0])) {
                if (DTSTAMP) { free(DTSTAMP[0]); free(DTSTAMP); }
                if (UID) { free(UID[0]); free(UID); }
                for (int i = 0; i < numLines; i++) free(entire_file[i]);
                free(entire_file);
                #if DEBUG
                    printf("Error! The time zone format is invalid.\n");
                #endif
                deleteCalendar(*obj);
                obj = NULL;
                return INV_DT;
            }

            char ** DTSTART = NULL;
            int dts = 0;
            __error__ = OK;
            DTSTART = findProperty(entire_file, beginLine, numLines, "DTSTART:", true, &dts, &__error__);

            if (DTSTART == NULL || strlen(DTSTART[0]) == 0) {
                /* if it exists */
                if (DTSTAMP) { free(DTSTAMP[0]); free(DTSTAMP); }
                if (DTSTART) {
                    free(DTSTART[0]); free(DTSTART);
                    if (UID) { free(UID[0]); free(UID); }
                    for (int i = 0; i < numLines; i++) free(entire_file[i]);
                    free(entire_file);
                    #if DEBUG
                        printf("Error! START DATE/TIME property was not found.\n");
                    #endif
                    deleteCalendar(*obj);
                    *obj = NULL;
                    return INV_DT;
                }
                __error__ = OK;
                DTSTART = findProperty(entire_file, beginLine, numLines, "DTSTART;", true, &dts, &__error__);
                if (DTSTART) {
                    free(DTSTART[0]); free(DTSTART);
                    if (UID) { free(UID[0]); free(UID); }
                    for (int i = 0; i < numLines; i++) free(entire_file[i]);
                    free(entire_file);
                    #if DEBUG
                        printf("Error! START DATE/TIME property was not found.\n");
                    #endif
                    deleteCalendar(*obj);
                    *obj = NULL;
                    return __error__ == OTHER_ERROR ? INV_FILE : INV_DT;
                }
                if (UID) { free(UID[0]); free(UID); }
                for (int i = 0; i < numLines; i++) free(entire_file[i]);
                free(entire_file);
                #if DEBUG
                    printf("Error! START DATE/TIME property was not found.\n");
                #endif
                deleteCalendar(*obj);
                *obj = NULL;
                return INV_EVENT;
            }
            /* We found it, but it's not valid */
            if (!validateStamp(DTSTART[0])) {
                if (DTSTAMP) { free(DTSTAMP[0]); free(DTSTAMP); }
                if (DTSTART) { free(DTSTART[0]); free(DTSTART); }
                if (UID) { free(UID[0]); free(UID); }
                for (int i = 0; i < numLines; i++) free(entire_file[i]);
                free(entire_file);
                #if DEBUG
                    printf("Error! The time zone format is invalid.\n");
                #endif
                deleteCalendar(*obj);
                *obj = NULL;
                return INV_DT;
            }

            Event * newEvent = NULL;
            newEvent = (Event *) calloc ( 1, sizeof(Event) );
            newEvent->properties = initializeList(printProperty, deleteProperty, compareProperties);
            /* Writing the date & time into their respective slots */
            int di = 0, ti = 0, index = 0;

            bool isUTC = (DTSTAMP[0][strlen(DTSTAMP[0]) - 1] == 'Z' ? true : false);
            for ( ; index < (isUTC ? strlen(DTSTAMP[0]) - 1 : strlen(DTSTAMP[0])); index++) {
                if (DTSTAMP[0][index] != 'T') {
                    if (index < 8) newEvent->creationDateTime.date[di++] = DTSTAMP[0][index];
                    else newEvent->creationDateTime.time[ti++] = DTSTAMP[0][index];
                }
            }
            newEvent->creationDateTime.UTC = isUTC;

            /* Start date is also required apparently */

            di = ti = index = 0;
            isUTC = (DTSTART[0][strlen(DTSTART[0]) - 1] == 'Z' ? true : false);
            for ( ; index < (isUTC ? strlen(DTSTART[0]) - 1 : strlen(DTSTART[0])); index++) {
                if (DTSTART[0][index] != 'T') {
                    if (index < 8) newEvent->startDateTime.date[di++] = DTSTART[0][index];
                    else newEvent->startDateTime.time[ti++] = DTSTART[0][index];
                }
            }

            newEvent->startDateTime.UTC = isUTC;

            /* Append null terminators */
            strcat(newEvent->creationDateTime.date, "\0");
            strcat(newEvent->creationDateTime.time, "\0");

            strcat(newEvent->startDateTime.date, "\0");
            strcat(newEvent->startDateTime.time, "\0");

            strcpy(newEvent->UID, UID[0]);

            if (DTSTAMP) { free(DTSTAMP[0]); free(DTSTAMP); };
            if (DTSTART) { free(DTSTART[0]); free(DTSTART); }
            if (UID) { free(UID[0]); free(UID); }

            /* Once that's done, we can start parsing out other information */
            int pCount = 0, errors = 0;
            char ** eventProperties = getAllPropertyNames(entire_file, beginLine, numLines, &pCount, &errors);

            if (errors || !pCount) {
                #if DEBUG
                    printf("Error! While retrieving properties of event\n");
                #endif
                for (int x = 0; x < pCount; x++) free(eventProperties[x]);
                if (eventProperties) free(eventProperties);
                for (int x = 0; x < numLines; x++) free(entire_file[x]);
                free(entire_file);
                deleteCalendar(*obj);
                *obj = NULL;
                return INV_EVENT;
            }

            for (int x = 0; x < pCount; x++) {
                /* We already have this one */
                if (strcasecmp(eventProperties[x], "UID:") && strcasecmp(eventProperties[x], "DTSTAMP:") && strcasecmp(eventProperties[x], "DTSTAMP;") && strcasecmp(eventProperties[x], "DTSTART:") && strcasecmp(eventProperties[x], "DTSTART;")) {

                    int valueCount = 0;
                    char ** value = findProperty(entire_file, beginLine, numLines, eventProperties[x], false, &valueCount, &__error__);

                    if (value == NULL) {
                        for (int x = 0; x < pCount; x++) free(eventProperties[x]);
                        free(eventProperties);
                        if (value) {
                            for (int ii = 0; ii < valueCount; ii++) free(value[ii]);
                            free(value);
                        }
                        for (int i = 0; i < numLines; i++) free(entire_file[i]);
                        free(entire_file);
                        if (newEvent->properties) freeList(newEvent->properties);
                        free(newEvent);
                        deleteCalendar(*obj);
                        *obj = NULL;
                        return INV_EVENT;
                    }

                    for (int vc = 0; vc < valueCount; vc++) {
                        /* Basically, incorrect value */
                        if (!value || !strlen(value[vc])) {
                            #if DEBUG
                                printf("Error! While retrieving value of event property\n");
                            #endif
                            for (int j = 0; j < valueCount; j++) free(value[j]);
                            free(value);
                            for (int j = 0; j < pCount; j++) free(eventProperties[j]);
                            free(eventProperties);
                            for (int j = 0; j < numLines; j++) free(entire_file[j]);
                            free(entire_file);
                            if (newEvent->properties) freeList(newEvent->properties);
                            free(newEvent);
                            deleteCalendar(*obj);
                            *obj = NULL;
                            return INV_EVENT;
                        }

                        Property * prop = (Property *) calloc (1, sizeof(Property) + strlen(value[vc]) + 1);
                        int j = 0;
                        for (; j < strlen(eventProperties[x]) - 1; j++)
                            prop->propName[j] = eventProperties[x][j];
                        prop->propName[j] = '\0';
                        strcpy(prop->propDescr, value[vc]);
                        insertBack(newEvent->properties, prop);
                    }
                    /* Free */
                    for (int vc = 0; vc < valueCount; vc++) free(value[vc]);
                    if (value) free(value);
                }
            }
            /* Free */
            for (int j = 0; j < pCount; j++) free(eventProperties[j]);
            free(eventProperties);

            /* Once we're done reading in the properties, we can begin reading in ALARMS */
            newEvent->alarms = initializeList(printAlarm, deleteAlarm, compareAlarms);
            for (int ai = beginLine + 1; ai < endLine; ai++) {
                if (!strcasecmp(entire_file[ai], "BEGIN:VALARM\r\n")) {
                    Alarm * alarm = createAlarm(entire_file, ai, endLine);
                    if (alarm == NULL) {
                        freeList(newEvent->alarms);
                        freeList(newEvent->properties);
                        free(newEvent);
                        for (int i = 0; i < numLines; i++) free(entire_file[i]);
                        free(entire_file);
                        #if DEBUG
                            printf("Error! Something wrong with the alarm.\n");
                        #endif
                        deleteCalendar(*obj);
                        *obj = NULL;
                        return INV_ALARM;
                    }
                    insertBack(newEvent->alarms, alarm);
                }
            }

            /* Insert the event at the back of the queue in the calendar object */
            insertBack((*obj)->events, newEvent);
            eventCount++;
            /* Once we're done */
            beginLine = endLine + 1;
        } else {
            beginLine++;
        }
    }

    for (int i = 0; i < numLines; i++) free(entire_file[i]);
    free(entire_file);

    /* There are no events */
    if (!eventCount) {
        #if DEBUG
            printf("There are no events in the calendar..");
        #endif
        deleteCalendar(*obj);
        *obj = NULL;
        return INV_CAL;
    }

    return OK;
}
/* Deletes the calendar */
void deleteCalendar(Calendar * obj) {
    if (obj == NULL) return;
    if (obj->properties) freeList( obj->properties );
    if (obj->events) freeList( obj->events );
    free ( obj );
    obj = NULL;
}
/* Reads tokens, returns -1 for index if the tokens are bad */
char * getToken ( char * entireFile, int * index, ICalErrorCode * errorCode) {
    char * line = NULL;
    int lineIndex = 0;
    /* Quick checks */
    if (entireFile == NULL || strlen(entireFile) == 0) return NULL;
    if (*index == strlen(entireFile)) return NULL;
    if (index == NULL || *index < 0) return NULL;

    int comment = (entireFile[*index] == ';' ? 1 : 0);

    /* Loop until the end */
    while (*index < strlen(entireFile)) {
        /* If we reached what seems to be a line ending */
        /* printf("Current character is %c\n", entireFile[*index]); */
        if (entireFile[*index] == '\r') {
            /* Check to make sure we can check the next character */
            if ((*index + 1) < strlen(entireFile)) {
                /* If it's a \n */
                if (entireFile[*index + 1] == '\n') {
                    /* Skip the next character */
                    *index += 2;
                    /* Return output */
                    return line;
                } else {
                    /* Basically a \r without \n inside a comment is fine */
                    if (comment) {
                        *index += 1;
                        return line;
                    }
                    /* Random \r??? */
                    *errorCode = INV_FILE;
                    #if DEBUG
                        printf("Encountered a backslash-r without backslash-n. Exiting.\n");
                    #endif
                    if (line) free(line);
                    /* Return NULL */
                    *index = -1;
                    return NULL;
                }
            } else {
                /* At the very end of the file, but started with a comment */
                if (comment) {
                    *index += 1;
                    return line;
                }
                *errorCode = INV_FILE;
                if (line) free(line);
                *index = -1;
                return NULL;
            }
        } else if (entireFile[*index] == '\n') {
            /* If it's a random \n, we should be good because it's inside the comment */
            if (comment) {
                *index += 1;
                return line;
            }
            *errorCode = INV_FILE;
            #if DEBUG
                printf("Encountered a backslash-n. Exiting.\n");
            #endif
            if (line) free(line);
            *index = -1;
            return NULL;
        } else {
            if (line == NULL) {
                line = (char *) calloc (1, 2 );
                line[lineIndex] = entireFile[*index];
                line[lineIndex + 1] = '\0';
            } else {
                line = (char *) realloc (line, lineIndex + 2);
                line[lineIndex] = entireFile[*index];
                line[lineIndex + 1] = '\0';
            }
            *index += 1;
            lineIndex += 1;
            /* Should be good */
        }
    }
    if (comment) {
        return line;
    }
    *errorCode = INV_FILE;

    #if DEBUG
        printf("Was not able to tokenize string, for unknown reason.\n");
    #endif
    if (line) free(line);
    *index = -1;
    return NULL;
}
/* Reads an entire file, tokenizes it and exports an array of lines */
char ** readFile ( char * fileName, int * numLines, ICalErrorCode * errorCode ) {
    /* Local variables */
    FILE * calendarFile = fopen(fileName, "r");
    *errorCode = OK;

    /* Even though we already checked for this, might as well just do this again */
    if (calendarFile == NULL) {
        *errorCode = INV_FILE;
        return NULL;
    }

    char buffer = EOF;
    char * entireFile = NULL;
    char ** returnFile = NULL;
    int index = 0, tokenSize, numTokens = 0;

    /* Read until we hit end of file */
    while ( 1 ) {
        buffer = fgetc(calendarFile);
        if (buffer == EOF) {
            break;
        }
        /* Realloc to insert new character */
        if (entireFile == NULL) {
            entireFile = (char *) calloc( 1, 1 );
            entireFile[0] = (char) buffer;
        } else {
            entireFile = realloc(entireFile, index + 2);
            entireFile[index] = (char) buffer;
            /* Adds backslash zero */
            entireFile[index + 1] = '\0';
        }
        /* Incremenet index either way */
        index++;
    }

    fclose(calendarFile);

    if (!index || !entireFile) {
        *errorCode = INV_FILE;
        return NULL;
    }
    /* Prepare the array */
    index = 0;
    /* Tokenizes the String */
    while ( 1 ) {
        char * token = getToken(entireFile, &index, errorCode);
        /* -1 means that something was bad */

        if (token == NULL && index != -1) {
            if (numTokens == 0) {
                *errorCode = INV_CAL;
                if (entireFile) free(entireFile);
                if (returnFile) {
                    for (int i = 0; i < numTokens; i++) free ( returnFile[i] );
                    free(returnFile);
                }
                return NULL;
            } else if (numTokens > 0 && !strcasecmp(returnFile[numTokens - 1], "END:VCALENDAR\r\n")) {
                if (token) free(token);
                break;
            } else {
                /* This is an empty line */
                #if DEBUG
                    printf("Invalid file token has been called\n");
                #endif
                if (returnFile == NULL) {
                    returnFile = calloc(1, sizeof(char *) );
                } else {
                    returnFile = realloc(returnFile, sizeof(char *) * (numTokens + 1));
                }
                returnFile[numTokens] = (char *) calloc(1, 3);
                strcpy(returnFile[numTokens++], "\r\n");
                *errorCode = checkFormatting(returnFile, numTokens, 1);
                if (entireFile) free(entireFile);
                if (returnFile) {
                    for (int i = 0; i < numTokens; i++) free ( returnFile[i] );
                    free(returnFile);
                }
                return NULL;
            }
            /* Basically it looks like an empty line */
        }
        if (*errorCode == INV_FILE) {
            if (entireFile) free(entireFile);
            for (int j = 0; j < numTokens; j++) free(returnFile[j]);
            if (entireFile) free(returnFile);
            return NULL;
        }

        tokenSize = strlen(token);

        if (index == -1 || tokenSize == 0) {
            #if DEBUG
                printf("Error: tokenizer returned: (index: %d, tokenSize: %d)\n", index, tokenSize);
                printf("Attempting to free all memory!\n");
            #endif
            /* Free everything */
            if (token) free (token);
            if (entireFile) free(entireFile);
            /* Whatever we've allocated before */
            /* Free it */
            if (returnFile) {
                for (int i = 0; i < numTokens; i++)
                    free ( returnFile[i] );
                free(returnFile);
            }
            #if DEBUG
                printf("Successfully freed all memory!\n");
            #endif
            return NULL;
        }
        /* printf("The token is (%s)\n", token); */
        /* Ignore comment */
        if (token[0] == ';') {
            free(token);
            continue;
        }
        *errorCode = OK;

        char * line = NULL;
        line = (char *) calloc(1, tokenSize + 3 );
        /* Create line from token, adding back in the delimeter */
        strcpy(line, token);
        free (token);
        /* Add line break, and \0 for the meme */
        line[tokenSize] = '\r';
        line[tokenSize + 1] = '\n';
        line[tokenSize + 2] = '\0';
        /* Allocate/Reallocate memory dynamically */
        if (returnFile == NULL) {
            returnFile = calloc(1, sizeof(char *) );
        } else {
            returnFile = realloc(returnFile, sizeof(char *) * (numTokens + 1));
        }
        /* Create enough space in that index for the line of text */
        returnFile[numTokens] = (char *) calloc (1,  strlen(line) + 1);
        strcpy(returnFile[numTokens], line);
        free(line);
        /* Add the \0 to indicate line ending */
        returnFile[numTokens][strlen(returnFile[numTokens])] = '\0';
        numTokens++;
    }
    /* Free memory & close File */
    *errorCode = OK;

    free(entireFile);
    *numLines = numTokens;
    /* END */
    return returnFile;
}
/* Extracts and returns an array of properties */
/* [Begin Index, End Index) */
char ** findProperty(char ** file, int beginIndex, int endIndex, char * propertyName, bool once, int * count, ICalErrorCode * __error__) {
    int li = beginIndex, opened = 0, closed = 0, currentlyFolding = 0, index = 0;
    char * result = NULL;

    /* Basically, returns an array of results */
    char ** results = NULL;
    int size = 0;

    int foundCount = 0;
    /* Doesn't start properly */
    if ( strcasecmp(file[li], "BEGIN:VCALENDAR\r\n") && strcasecmp(file[li], "BEGIN:VALARM\r\n") && strcasecmp(file[li], "BEGIN:VEVENT\r\n") )
        return NULL;
    while (li < endIndex) {
        if ( !strcasecmp(file[li], "BEGIN:VCALENDAR\r\n") || !strcasecmp(file[li], "BEGIN:VALARM\r\n") || !strcasecmp(file[li], "BEGIN:VEVENT\r\n") ) {
            opened++;
        } else if ( !strcasecmp(file[li], "END:VCALENDAR\r\n") || !strcasecmp(file[li], "END:VALARM\r\n") || !strcasecmp(file[li], "END:VEVENT\r\n") ) {
            closed++;
            if (closed == opened) {
                /* If we have reached the end of the current level */
                if (once) {
                    if (foundCount == 1) {
                        /* Whatever the first result was */
                        if (result) free(result);
                        return results;
                    } else {
                        if (result) free(result);
                        for (int j = 0; j < size; j++) free(results[j]);
                        if (results) free(results);
                        *count = 0;
                        if (foundCount > 1)
                            *__error__ = OTHER_ERROR; /* duplicate */
                        return NULL;
                    }
                } else {
                    if (result) free(result);
                    *count = size;
                    return results;
                }
            }
        } else {
            /* If we're on the current layer */
            if (opened - 1 == closed) {
                if ((file[li][0] == '\t' || file[li][0] == ' ') && currentlyFolding) {
                    for (int j = 1; j < strlen(file[li]) - 2; j++) {
                        if (!result) result = calloc(1, 2);
                        else result = realloc(result, strlen(result) + 2);
                        result[index] = file[li][j];
                        result[index + 1] = '\0';
                        index++;
                    }
                    if (li + 1 < endIndex) {
                        if (file[li + 1][0] == ' ' || file[li + 1][0] == '\t') {
                            currentlyFolding = 1;
                        } else {
                            /* If it's empty or something */
                            if (result == NULL || strlen(result) == 0) {
                                for (int j = 0; j < size; j++) free(results[j]);
                                if (results) free(results);
                                results = (char **) calloc ( 1, sizeof(char *));
                                results[0] = (char *) calloc ( 1, 1);
                                if (result) free(result);
                                *count = 1;
                                return results;
                            }
                            foundCount++;
                            /* Dynamic allocation */
                            if (results == NULL) {
                                results = calloc ( 1, sizeof (char *) );
                            } else {
                                results = realloc ( results, (size + 1) * sizeof(char *) );
                            }
                            results[size] = (char *) calloc(1, strlen(result) + 1);
                            /* Essentially copy whatever into the 2d array, and re-set the string */
                            strcpy(results[size], result);
                            strcpy(result, "");
                            index = 0;
                            currentlyFolding = 0;
                            size++;
                        }
                    } else {
                        if (once) {
                            if (foundCount == 1) {
                                if (result) free(result);
                                return results;
                            } else {
                                if (result) free(result);
                                for (int j = 0; j < size; j++) free(results[j]);
                                if (results) free(results);
                                *count = 0;
                                if (foundCount > 1)
                                    *__error__ = OTHER_ERROR;
                                return NULL;
                            }
                        } else {
                            if (result) free(result);
                            *count = size;
                            return results;
                        }
                    }
                } else {
                    if (strlen(propertyName) <= strlen(file[li]) - 2) {
                        int match = 1, j = 0;
                        for (; j < strlen(propertyName); j++)
                            if (propertyName[j] != file[li][j]) match = 0;
                        if (match) {
                            for (; j < strlen(file[li]) - 2; j++) {
                                if (!result) result = calloc(1, 2);
                                else result = realloc(result, strlen(result) + 2);
                                result[index] = file[li][j];
                                result[index + 1] = '\0';
                                index++;
                            }
                            if (li + 1 < endIndex) {
                                if (file[li + 1][0] == ' ' || file[li + 1][0] == '\t') {
                                    currentlyFolding = 1;
                                } else {
                                    foundCount++;
                                    /* If it's empty or something */
                                    if (result == NULL || strlen(result) == 0) {
                                        for (int j = 0; j < size; j++) free(results[j]);
                                        if (results) free(results);
                                        results = (char **) calloc ( 1, sizeof(char *));
                                        results[0] = (char *) calloc ( 1, 1);
                                        if (result) free(result);
                                        *count = 1;
                                        return results;
                                    }
                                    /* Dynamic allocation */
                                    if (results == NULL) {
                                        results = calloc ( 1, sizeof (char *) );
                                    } else {
                                        results = realloc ( results, (size + 1) * sizeof(char *) );
                                    }
                                    results[size] = (char *) calloc(1, strlen(result) + 1);
                                    /* Essentially copy whatever into the 2d array, and re-set the string */
                                    strcpy(results[size], result);
                                    strcpy(result, "");
                                    index = 0;
                                    currentlyFolding = 0;
                                    size++;
                                }
                            } else {
                                if (once) {
                                    if (foundCount == 1) {
                                        if (result) free(result);
                                        *count = size;
                                        return results;
                                    } else {
                                        if (result) free(result);
                                        for (int j = 0; j < size; j++) free(results[j]);
                                        if (results) free(results);
                                        *count = 0;
                                        return NULL;
                                    }
                                } else {
                                    if (result) free(result);
                                    *count = size;
                                    return results;
                                }
                            }
                        }
                    }
                }
            }
        }
        li++;
    }
    /* NOT too sure what to do here so we just did the basic clean up */
    if (result) free(result);
    for (int j = 0; j < size; j++) free(results[j]);
    if (results) free(results);
    *count = 0;
    return NULL;
}
/* Verifies that all alarms & events are distributed correctly */
ICalErrorCode checkFormatting (char ** entire_file, int numLines, int ignoreCalendar ) {
    if (numLines < 2) return INV_FILE;

    /* This is a bit of a work-around, but if we have an empty line we need to check where it's messing up */
    /* This is why we have this */
    if (strcasecmp(entire_file[0], "BEGIN:VCALENDAR\r\n")) {
        return INV_CAL;
    }

    if (!ignoreCalendar) {
        if (strcasecmp(entire_file[numLines - 1], "END:VCALENDAR\r\n")) {
            return INV_CAL;
        }
    }

    int isEventOpen = 0;
    int isAlarmOpen = 0;
    int numEventsOpened = 0, numEventsClosed = 0;
    int numAlarmsOpened = 0, numAlarmsClosed = 0;

    for (int i = 1; i < (ignoreCalendar ? numLines : numLines - 1); i++) {
        if (strcasecmp(entire_file[i], "BEGIN:VEVENT\r\n") == 0) {
            if (isEventOpen) return INV_EVENT; /* There's an open event */
            if (isAlarmOpen) return INV_ALARM; /* There's an open alarm */

            isEventOpen = 1;
            numEventsOpened++;
        } else if (strcasecmp(entire_file[i], "END:VEVENT\r\n") == 0) {
            if (!isEventOpen) return INV_CAL; /* We have no ongoing events */
            if (isAlarmOpen) return INV_ALARM; /* There's an alarm open */
            if (numAlarmsOpened != numAlarmsClosed) {
                return INV_ALARM;
            }
            /* Each time we close an event we re-set alarms */
            numAlarmsOpened = numAlarmsClosed = 0;
            isEventOpen = 0;
            numEventsClosed++;
        } else if (strcasecmp(entire_file[i], "BEGIN:VALARM\r\n") == 0) {
            if (isAlarmOpen) return INV_ALARM;
            if (!isEventOpen) return INV_CAL;

            isAlarmOpen = 1;
            numAlarmsOpened++;
        } else if (strcasecmp(entire_file[i], "END:VALARM\r\n") == 0) {
            if (!isAlarmOpen) return INV_EVENT;
            if (!isEventOpen) return INV_CAL;

            isAlarmOpen = 0;
            numAlarmsClosed++;
        } else if (entire_file[i][0] != ' ' && entire_file[i][0] != '\t' && entire_file[i][0] != ';') {
            int lineLength = strlen(entire_file[i]);
            if (lineLength <= 2) {
                if (isAlarmOpen) return INV_ALARM;
                if (isEventOpen) return INV_EVENT;
                return INV_CAL;
            }
            if (toupper(entire_file[i][0]) == 'B' && toupper(entire_file[i][1]) == 'E' && toupper(entire_file[i][2]) == 'G' && toupper(entire_file[i][3]) == 'I' && toupper(entire_file[i][4]) == 'N' && (entire_file[i][5]) == ':') {
                if (isAlarmOpen) return INV_ALARM;
                if (isEventOpen) return INV_EVENT;
                return INV_CAL;
            }
            if (toupper(entire_file[i][0]) == 'E' && toupper(entire_file[i][1]) == 'N' && toupper(entire_file[i][2]) == 'D' && entire_file[i][3] == ':'){
                if (isAlarmOpen) return INV_ALARM;
                if (isEventOpen) return INV_EVENT;
                return INV_CAL;
            }
            int hasColon = 0, insideQuotes = 0;
            for (int j = 1; j < lineLength; j++) {
                if (entire_file[i][j] == '"') insideQuotes ^= 1;
                if (!insideQuotes) {
                    if (entire_file[i][j] == ':') hasColon++;
                    else if (entire_file[i][j] == ';') {
                        /* Attach && Trigger */
                        if ((j - 6) >= 0 && (entire_file[i][j - 1] == 'h' || entire_file[i][j - 1] == 'H') && (entire_file[i][j - 2] == 'c' || entire_file[i][j - 2] == 'C')
                        && (entire_file[i][j - 3] == 'a' || entire_file[i][j - 3] == 'A') && (entire_file[i][j - 4] == 't' || entire_file[i][j - 4] == 'T')
                        && (entire_file[i][j - 5] == 't' || entire_file[i][j - 5] == 'T') && (entire_file[i][j - 6] == 'a' || entire_file[i][j - 6] == 'A')) {
                            hasColon++;
                        } else if ((j - 7) >= 0 && (entire_file[i][j - 1] == 'r' || entire_file[i][j - 1] == 'R') && (entire_file[i][j - 2] == 'e' || entire_file[i][j - 2] == 'E')
                        && (entire_file[i][j - 3] == 'g' || entire_file[i][j - 3] == 'G') && (entire_file[i][j - 4] == 'g' || entire_file[i][j - 4] == 'G')
                        && (entire_file[i][j - 5] == 'i' || entire_file[i][j - 5] == 'I') && (entire_file[i][j - 6] == 'r' || entire_file[i][j - 6] == 'R')
                        && (entire_file[i][j - 7] == 't' || entire_file[i][j - 7] == 'T')) {
                            hasColon++;
                        }
                    }
                }
            }
            /* It's folded */
            if (!hasColon) {
                if (isAlarmOpen) return INV_ALARM;
                if (isEventOpen) return INV_EVENT;
                return INV_CAL;
            }
        }
    }
    if (isAlarmOpen || numAlarmsOpened != numAlarmsClosed) return INV_ALARM;
    if (isEventOpen || numEventsOpened != numEventsClosed) return INV_EVENT;

    return OK;
}
/* prints the calendar */
char * printCalendar (const Calendar * obj) {
    char * result = NULL;

    if (obj == NULL) return NULL;
    int length = 10;

    result = (char *) calloc ( 1, length );
    strcpy(result, "CALENDAR\n");
    length += (8 + strlen(obj->prodID) + 1);
    result = (char *) realloc (result, length);
    strcat(result, "PRODID:");
    strcat(result, obj->prodID);
    strcat(result, "\n");

    char version[200 + 10];
    sprintf(version, "VERSION:%f", obj->version);
    length += strlen(version) + 1;
    result = (char *) realloc (result, length);
    strcat(result, version);

    char * otherProps = NULL;
    otherProps = toString(obj->properties);
    length += strlen(otherProps) + 2;
    result = (char *) realloc (result, length);
    strcat(result, otherProps);
    strcat(result, "\n");

    free(otherProps);

    /* At this point we have all calendar properties parsed and ready to go */
    /* This is where we going to have to implement events */

    length += 7;
    result = (char *) realloc( result, length);
    strcat(result, "EVENTS");

    char * events = NULL;
    events = toString(obj->events);
    length += strlen(events) + 2;
    result = (char *) realloc ( result, length);
    strcat(result, events);
    strcat(result, "\n");

    free(events);

    return result;
}
/* Returns a list of strings representing property names given an index */
/* All strings are terminated with \0 and nothing else */
char ** getAllPropertyNames (char ** file, int beginIndex, int endIndex, int * numElements, int * errors) {
    int index = beginIndex;
    int opened = 0;
    int closed = 0;
    char ** result = NULL;

    /* Must start with begin or end index */
    if ( strcasecmp(file[index], "BEGIN:VCALENDAR\r\n") && strcasecmp(file[index], "BEGIN:VALARM\r\n") && strcasecmp(file[index], "BEGIN:VEVENT\r\n") ) {
        return NULL;
    }

    while (index < endIndex) {
        char * ln = file[index];
        /* BEGIN */
        if (toupper(ln[0]) == 'B' && toupper(ln[1]) == 'E' && toupper(ln[2]) == 'G' && toupper(ln[3]) == 'I' && toupper(ln[4]) == 'N') {
            opened++;
        /* END */
        } else if (toupper(ln[0]) == 'E' && toupper(ln[1]) == 'N' && toupper(ln[2]) == 'D') {
            closed++;
            if (opened == closed) {
                break;
            }
        /* FOLDED */
        } else if (ln[0] == ' ' || ln[0] == '\t' || ln[0] == ';') {
        } else {
            if (opened - 1 == closed) {
                int colonIndex = 0, foundColon = 0;
                /* Find colon index */
                for (int i = 0; i < strlen(ln);  i++) {
                    colonIndex++;
                    if (ln[i] == ':' || ln[i] == ';') {
                        foundColon = 1;
                        break;
                    }
                }
                /* if we have not found a colon of any sort it's an error */
                *errors += !foundColon;

                if (*errors) {
                    #if DEBUG
                        printf("Seems that there is a content line that is missing a colon of sorts\n");
                    #endif
                    for (int j = 0; j < *numElements; j++)
                        free ( result[j] );
                    if (result) free(result);
                    return NULL;
                }

                char * line = NULL;
                line = (char *) calloc (1, colonIndex + 1);

                for (int i = 0; i < colonIndex; i++) {
                    line[i] = ln[i];
                }
                strcat(line, "\0");

                int repeat = 0;

                /* Make sure there's no duplicates */
                for (int check = 0; check < *numElements; check++) {
                    if (!strcasecmp(line, result[check])) {
                        repeat = 1;
                        free(line);
                        break;
                    }
                }

                if (!repeat) {
                    /* After we found the event name */
                    if (result == NULL) {
                        result = calloc (1, sizeof (char *) );
                    } else {
                        result = realloc ( result, sizeof (char *) * ( *numElements + 1) ) ;
                    }
                    /* Copy it into the resulting array */
                    int strLen = strlen(line) + 1;
                    result[*numElements] = NULL;
                    result[*numElements] = calloc(1, strLen );
                    strcpy(result[*numElements], line);
                    /* Free & append the null terminator */
                    free(line);
                    strcat(result[*numElements], "\0");
                    /*
                    result[*numElements][strlen(result[*numElements])] = '\0';
                    */
                    /* Increase the size of array */
                    *numElements += 1;
                }
            }
        }
        index++;
    }
    return result;
}
/* <-----HELPER FUNCTIONS------> */
/* EVENTS */
void deleteEvent(void* toBeDeleted) {
    if (toBeDeleted == NULL) return;
    Event * e = (Event *) toBeDeleted;
    if (e->properties) freeList(e->properties);
    if (e->alarms) freeList(e->alarms);
    free(e);
}

void deleteAlarm(void* toBeDeleted) {
    if (toBeDeleted == NULL) return;
    Alarm * a = (Alarm *) toBeDeleted;
    free(a->trigger);
    if (a->properties) freeList(a->properties);
    free(a);
}

int compareEvents(const void* first, const void* second) {
    return 0;
}

/* Temporary print */
char* printEvent(void* toBePrinted) {
    if (toBePrinted == NULL) return NULL;

    Event * event = NULL;
    char * result = NULL;

    event = (Event *) toBePrinted;
    result = (char *) calloc(1, 22);
    strcpy(result, "\t=BEGIN EVENT=\n\tUID->");
    result = (char *) realloc( result, strlen(result) + strlen(event->UID) + 1);
    strcat(result, event->UID);
    result = (char *) realloc (result, strlen(result) + 12);
    strcat(result, "\n\tDTSTAMP->");

    char * dt_stamp = printDate(&event->creationDateTime);
    char * dt_start = printDate(&event->startDateTime);

    result = (char *) realloc (result, strlen(result) + strlen(dt_stamp) + 1);
    strcat(result, dt_stamp);
    free(dt_stamp);

    result = (char *) realloc(result, strlen(result) + strlen(dt_start) + 12);
    strcat(result, "\n\tDTSTART->");
    strcat(result, dt_start);
    free(dt_start);

    char * otherProps = NULL;
    otherProps = toString(event->properties);
    result = (char *) realloc (result, strlen(result) + strlen(otherProps) + 1);
    strcat(result, otherProps);
    free(otherProps);

    char * alarms = NULL;
    alarms = toString(event->alarms);
    result = (char *) realloc (result, strlen(result) + strlen(alarms) + 2);
    strcat(result, alarms);
    free(alarms);

    strcat(result, "\n");
    result = (char *) realloc(result, strlen(result) + 13);
    strcat(result, "\t=END EVENT=");

    return result;
}

char* printAlarm(void* toBePrinted) {
    if (toBePrinted == NULL) return NULL;
    Alarm * a = (Alarm *) toBePrinted;
    char * output = (char *) calloc ( 1, 16);
    strcpy(output, "\t=BEGIN ALARM=\n");
    output = (char *) realloc (output, strlen(output) + strlen(a->action) + 1 + 1 + 8 + 1 );
    strcat(output, "\tACTION->");
    strcat(output, a->action);
    strcat(output, "\n");
    output = (char *) realloc (output, strlen(output) + strlen(a->trigger) + 1 + 1 + 9 );
    strcat(output, "\tTRIGGER->");
    strcat(output, a->trigger);
    /* also properties */
    char * otherProps = NULL;
    otherProps = toString(a->properties);
    output = (char *) realloc (output, strlen(output) + strlen(otherProps) + 1);
    strcat(output, otherProps);

    free(otherProps);
    output = (char *) realloc(output, strlen(output) + 14);
    strcat(output, "\n\t=END ALARM=");
    return output;
}
/* PROPERTIES */
void deleteProperty(void* toBeDeleted) {
    if (toBeDeleted == NULL) return;
    Property * p = (Property *) toBeDeleted;
    /* We don't have to free the flexible array */
    free(p);
}

int compareProperties(const void* first, const void* second) {
    return 0;
}

int compareAlarms(const void * first, const void * second) {
    return 0;
}
/* Prints an individual property */
char* printProperty(void* toBePrinted) {
    if (toBePrinted == NULL) return NULL;
    Property * p = (Property *) toBePrinted;
    /* Allocates enough space for the property name and null-terminator */
    char * result = NULL;
    result = (char *) calloc ( 1, strlen(p->propName) + 2 );
    /* Copies the property name into the resulting string */
    strcpy(result, "\t");
    strcat(result, p->propName);
    /* Allocates some more memory for the property description */
    result = (char *) realloc (  result, strlen(result) + strlen(p->propDescr) + 3);
    /* Appends the description onto the result */
    strcat(result, "->");
    strcat(result, p->propDescr);
    /* Returns result */
    return result;
}
/* Not complete yet, but it does check for the correct date format */
int validateStamp ( char * check ) {
    int to = 0, len = 0,  mi = 0, di = 0, index = 0;

    len = strlen(check);
    for (int x = 0; x < len; x++) {
        if (check[x] == 'T') {
            to++;
        }
    }
    /* We need to know where the date ends */
    if (to != 1) return 0;

    char month[2];
    char day[2];

    while (index < 4) {
        if ( !isdigit(check[index++]) )
            return 0;
    }

    while (index < 4 + 2) {
        if ( !isdigit(check[index]) )
            return 0;
        month[mi++] = check[index++];
    }

    while (index < 4 + 2 + 2) {
        if ( !isdigit(check[index]) )
            return 0;
        day[di++] = check[index++];
    }
    /* So at this point we know that the day is correct and that,
    if the lenght is exactly 8 then the date is correct */

    if (check[index] != 'T') return 0;
    index++;

    /* NO LEAP YEARS??? */
    /* Might have to check each month individually */
    if (month[0] >= '2' || (month[0] == '1' && month[1] >= '3') || (month[0] == '0' && month[1] == '0')) return 0;
    if ( (day[0] == '0' && day[1] == '0') || (day[0] == '3' && day[1] >= '2') || day[0] >= '4' ) return 0;

    if ( index + 6 > strlen(check) ) return 0;

    char hour[2] = { check[index], check[index + 1] };
    char minute[2] = { check[index + 2], check[index + 3] };
    char second[2] = { check[index + 4], check[index + 5] };
    /* UTC token and size of check */
    if ( index + 6 < strlen(check) )
        if (check[index + 6] != 'Z' || (index + 7) < strlen(check)) return 0;

    if ( (hour[0] == '2' && hour[1] >= '4') || (hour[0] >= '3') ) return 0;
    if ( minute[0] >= '6' ) return 0;
    if ( second[0] >= '6' ) return 0;
    /* For now */
    return 1;
}
/* This creates an alarm object */
Alarm * createAlarm(char ** file, int beginIndex, int endIndex) {
    /* gotta make sure that the line begins correctly */
    if (strcasecmp(file[beginIndex], "BEGIN:VALARM\r\n")) return NULL;
    Alarm * alarm = (Alarm *) calloc ( 1, sizeof (Alarm) );
    alarm->properties = initializeList(printProperty, deleteProperty, compareProperties);
    /* Allocated memory for an alarm object */
    int numProps = 0, errors = 0;
    /* Grab alarm properties */
    char ** pnames = getAllPropertyNames(file, beginIndex, endIndex, &numProps, &errors);

    if (errors || !numProps) {
        #if DEBUG
            printf("Error! While retrieving properties of event (inside an alarm)\n");
        #endif
        for (int x = 0; x < numProps; x++) free(pnames[x]);
        if (pnames) free(pnames);
        freeList(alarm->properties);
        free(alarm);
        return NULL;
    }

    int ftrig = 0;
    int faction = 0;

    ICalErrorCode errorCode = OK;

    /* Run through props */
    for (int i = 0; i < numProps; i++) {
        /* We need to check some required ones here */
        int nump = 0;
        if (!strcasecmp(pnames[i], "ACTION:") || !strcasecmp(pnames[i], "ACTION;")) {
            char ** pval = findProperty(file, beginIndex, endIndex, pnames[i], true, &nump, &errorCode);
            /* Either not found, or bad, or more than one */
            if (pval == NULL) {
                for (int j = 0; j < numProps; j++) free(pnames[j]);
                free(pnames);
                freeList(alarm->properties);
                if (alarm->trigger) free(alarm->trigger);
                free(alarm);
                return NULL;
            }
            /* All goodie */
            strcpy(alarm->action, pval[0]);
            faction++;
            free(pval[0]);
            free(pval);
        } else if ( !strcasecmp (pnames[i], "TRIGGER:") || !strcasecmp(pnames[i], "TRIGGER;")) {
            char ** pval = findProperty(file, beginIndex, endIndex, pnames[i], true, &nump, &errorCode);
            if (pval == NULL) {
                for (int j = 0; j < numProps; j++) free(pnames[j]);
                free(pnames);
                freeList(alarm->properties);
                if (alarm->trigger) free(alarm->trigger);
                free(alarm);
                return NULL;
            }
            /* All goodie */
            alarm->trigger = (char *) calloc ( 1, strlen(pval[0]) + 1);
            strcpy(alarm->trigger, pval[0]);
            ftrig++;
            free(pval[0]);
            free(pval);
        } else {
            /* Handle others */
            char ** pval = findProperty(file, beginIndex, endIndex, pnames[i], false, &nump, &errorCode);
            if (pval == NULL || !strlen(pval[0])) {
                for (int j = 0; j < numProps; j++) free(pnames[j]);
                if (pval) {
                    for (int ii = 0; ii < nump; ii++) free(pval[ii]);
                    free(pval);
                }
                free(pnames);
                freeList(alarm->properties);
                if (alarm->trigger) free(alarm->trigger);
                free(alarm);
                return NULL;
            }
            for (int j = 0; j < nump; j++) {
                Property * prop = (Property *) calloc ( 1, sizeof(Property) + strlen(pval[j]) + 1);

                /* I forgot that property names STILL HAVE DELIMETER!!!! */
                strncpy(prop->propName, pnames[i], strlen(pnames[i]) - 1);
                /* arghhhghghg */
                strcpy(prop->propDescr, pval[j]);
                insertBack(alarm->properties, prop);
            }
            /* Free it, we don't need it anymore */
            for (int j = 0; j < nump; j++) free(pval[j]);
            free(pval);
        }
    }
    for (int i = 0; i < numProps; i++) free(pnames[i]);
    free(pnames);

    if (ftrig == 1 && faction == 1) return alarm;

    freeList(alarm->properties);
    if (alarm->trigger) free(alarm->trigger);
    free(alarm);
    return NULL;
}

void deleteDate(void* toBeDeleted) {
    if (toBeDeleted == NULL) return;

    DateTime * date = (DateTime *) toBeDeleted;
    strcpy(date->date, "\0");
    strcpy(date->time, "\0");
    date->UTC = false;
}

int compareDates(const void* first, const void* second) { return 0; }

char* printDate(void* toBePrinted) {
    if (toBePrinted == NULL) return NULL;
    /* Check */
    DateTime * date = (DateTime *) toBePrinted;
    char * output = (char *) calloc(1, strlen(date->date) + strlen(date->time) + 1 + 10);
    strcpy(output, date->date);
    strcat(output, ":");
    strcat(output, date->time);
    if (date->UTC) strcat(output, "(UTC)");
    return output;
}

ICalErrorCode writeCalendar(char* fileName, const Calendar* obj){
    /* Basic error checking */
    if (fileName == NULL || strlen(fileName) == 0 || obj == NULL)
        return WRITE_ERROR;
    /* freopen is more fun */
    FILE * fp = fopen(fileName, "w");
    if (fp == NULL) return WRITE_ERROR;
    fprintf(fp, "BEGIN:VCALENDAR\r\n");
    /* Print the important properties */
    char * temp;
    fprintf(fp, "VERSION:%.1f\r\n", obj->version);
    temp = getProp(obj->prodID);
    fprintf(fp, "PRODID%s\r\n", temp);
    free(temp);
    /* We now gotta print out calendar properties */
    ListIterator propertyIterator = createIterator(obj->properties);
    Property * prop = nextElement(&propertyIterator);
    while (prop != NULL) {
        temp = getProp(prop->propDescr);
        fprintf(fp, "%s%s\r\n", prop->propName, temp);
        free(temp);
        prop = nextElement(&propertyIterator);
    }
    /* Done with calendar's properties */
    ListIterator eventIterator = createIterator(obj->events);
    Event * event = nextElement(&eventIterator);
    while (event != NULL) {
        fprintf(fp, "BEGIN:VEVENT\r\n");
        /* We gotta print out the event's properties */
        /* Now we can go through the alarms */
        temp = getProp(event->UID);
        fprintf(fp, "UID%s\r\n", temp);
        free(temp);

        fprintf(fp, "DTSTAMP:%sT%s%s\r\n", event->creationDateTime.date, event->creationDateTime.time, event->creationDateTime.UTC ? "Z" : "");
        fprintf(fp, "DTSTART:%sT%s%s\r\n", event->startDateTime.date, event->startDateTime.time, event->startDateTime.UTC ? "Z" : "");
        /* Done printing requied properties */
        ListIterator ePropIterator = createIterator(event->properties);
        prop = nextElement(&ePropIterator);
        /* All the other properties */
        while (prop != NULL) {
            temp = getProp(prop->propDescr);
            fprintf(fp, "%s%s\r\n", prop->propName, temp);
            free(temp);
            prop = nextElement(&ePropIterator);
        }
        /* Once we're done doing that, we can finally move onto alarms */
        ListIterator alarms = createIterator(event->alarms);
        Alarm * alarm = nextElement(&alarms);
        /* Rip through alarms */
        while (alarm != NULL) {
            fprintf(fp, "BEGIN:VALARM\r\n");
            temp = getProp(alarm->action);
            fprintf(fp, "ACTION%s\r\n", temp);
            free(temp);
            temp = getProp(alarm->trigger);
            fprintf(fp, "TRIGGER%s\r\n", temp);
            free(temp);
            /* Now that we're done with that, we can rip through the rest of its' properties */
            ListIterator aProps = createIterator(alarm->properties);
            prop = nextElement(&aProps);
            while (prop != NULL) {
                temp = getProp(prop->propDescr);
                fprintf(fp, "%s%s\r\n", prop->propName, temp);
                free(temp);
                prop = nextElement(&aProps);
            }
            /* Donezo */
            fprintf(fp, "END:VALARM\r\n");
            alarm = nextElement(&alarms);
        }
        /* Now we're done with this event */
        fprintf(fp, "END:VEVENT\r\n");
        event = nextElement(&eventIterator);
    }
    fprintf(fp, "END:VCALENDAR\r\n");
    /* We're donezo */
    fclose(fp);
    return OK;
}

char * getProp (const char * prop) {
    int l = strlen(prop), quote = 0, hasParams = 0;
    /* Check for param evidence */
    for (int j = 0; j < l; j++) {
        if (prop[j] == '"') quote ^= 1;
        if (prop[j] == ':' && !quote) {
            /* Basically if there's a colon -- not inside quotes */
            /* This almost feels too basic lol */
            hasParams = 1;
            break;
        }
    }
    /* Allocate some space for \0 and the delimeter */
    char * result = (char *) calloc(1, strlen(prop) + 2 );
    result[0] = (hasParams ? ';' : ':');
    strcat(result, prop);
    return result;
}

int validLength(const char * string, int length) {
    if (string == NULL || string[0] == '\0' || !strlen(string)) return 0;
    /* Loop until specified length, see if we can find a NULL terminator */
    for (int i = 0; i < length; i++)
        if (string[i] == '\0')
            return 1;
    /* There is no NULL terminator within given length */
    return 0;
}

ICalErrorCode validateCalendar(const Calendar* obj) {
    /* If the object is NULL, we can just return invalid calendar */
    if (obj == NULL)
        return INV_CAL;

    /* If the prodID is empty */
    if ( !validLength(obj->prodID, 1000) ) {
        #if DEBUG
            printf("Error! PRODID seems to be invalid length!\n");
        #endif
        return INV_CAL;
    }

    /* Either list is badly initialized (or events is empty) */
    if (getLength(obj->events) <= 0 || getLength(obj->properties) < 0 ) {
        #if DEBUG
            printf("Error! Either events or properties (for calendar) have not been initialized!\n");
        #endif
        return INV_CAL;
    }

    int calscale = 0, method = 0;

    /* If some properties exist */
    if (getLength(obj->properties)) {
        ListIterator propertyIterator = createIterator(obj->properties);
        Property * prop = nextElement(&propertyIterator);

        while (prop != NULL) {
            /* Checking for null & empty strings */
            if ( !validLength(prop->propName, 200) || prop->propDescr == NULL || !strlen(prop->propDescr) ) {
                #if DEBUG
                    printf("Error! Either propname/propDescr are invalid size\n");
                #endif
                return INV_CAL;
            }
            /* Duplicate required properties */
            if (!strcasecmp(prop->propName, "PRODID") || !strcasecmp(prop->propName, "VERSION")) {
                #if DEBUG
                    printf("Error! PRODID or VERSION have appeared in optional properties!\n");
                #endif
                return INV_CAL;
            }
            if (!strcasecmp(prop->propName, "CALSCALE")) {
                if (calscale) {
                    #if DEBUG
                        printf("Error! Calscale appeared more than once\n");
                    #endif
                    return INV_CAL;
                }
                calscale++;
            } else if (!strcasecmp(prop->propName, "METHOD")) {
                if (method) {
                    #if DEBUG
                        printf("Error! Method appeared more than once\n");
                    #endif
                    return INV_CAL;
                }
                method++;
            } else {
                #if DEBUG
                    printf("Error! %s is not supposed to be inside calendar\n", prop->propName);
                #endif
                return INV_CAL;
            }
            prop = nextElement(&propertyIterator);
        }
    }
    /* So, at this point we have checked the calendar object shell */
    /* We can now start looking at events */

    ListIterator eventIterator = createIterator(obj->events);
    Event * event = nextElement(&eventIterator);

    while (event != NULL) {
        /* UID can't be empty */
        if (!validLength(event->UID, 1000)) {
            #if DEBUG
                printf("Error! Event UID is invalid length\n");
            #endif
            return INV_EVENT;
        }
        /* Either list is NULL, not empty */
        if (getLength(event->properties) < 0 || getLength(event->alarms) < 0) {
            #if DEBUG
                printf("Error! Event property/alarm list have not been initialized!\n");
            #endif
            return INV_EVENT;
        }

        /* Let's check dates */
        if (!validLength(event->creationDateTime.date, 9) || !validLength(event->creationDateTime.time, 7)) {
            #if DEBUG
                printf("Error! creation date/time is invalid!\n");
            #endif
            return INV_EVENT;
        }

        if (!validLength(event->startDateTime.date, 9) || !validLength(event->startDateTime.time, 7)) {
            #if DEBUG
                printf("Error! start date/time is invalid!\n");
            #endif
            return INV_EVENT;
        }

        /* If the event has some properties, let's check them out */
        if (getLength(event->properties)) {

            int hasEnd = 0;
            int hasDuration = 0;

            ListIterator propertyIterator = createIterator(event->properties);
            Property * prop = nextElement(&propertyIterator);

            /* Loop through some properties */
            while (prop != NULL) {
                /* Checking for null & empty strings */
                if (strlen(prop->propName) == 0 || prop->propDescr == NULL || strlen(prop->propDescr) == 0)
                    return INV_EVENT;

                /* Duplicates */
                if (!strcasecmp(prop->propName, "DTSTART") || !strcasecmp(prop->propName, "DTSTAMP") || !strcasecmp(prop->propName, "UID"))
                    return INV_EVENT;

                /* If we come across an end */
                if (!strcasecmp(prop->propName, "DTEND")) {
                    if (hasDuration || hasEnd)
                        return INV_EVENT;
                    hasEnd = 1;
                }
                /* If we come across a duration */
                else if (!strcasecmp(prop->propName, "DURATION")) {
                    if (hasEnd || hasDuration)
                        return INV_EVENT;
                    hasDuration = 1;
                } else {
                    /* Here we can loop through and check if it's a valid event */
                    int found = 0;
                    for (int j = 0; j < 25; j++) {
                        /* If we know the property is only supposed to occur once */
                        if ( eventprops[j][strlen(eventprops[j]) - 1] == '1' ) {
                            char * cmp = calloc(1, strlen(eventprops[j]) );
                            strncpy(cmp, eventprops[j], strlen(eventprops[j]) - 1);
                            /* Compare up */
                            if (!strcasecmp(cmp, prop->propName)) {
                                int count = 0;
                                ListIterator li = createIterator(event->properties);
                                Property * p = nextElement(&li);
                                /* Loop through the list and count properties */
                                while ( p != NULL) {
                                    /* We only gonna check the valid once to make things easier */
                                    if (p->propName && p->propDescr && strlen(p->propName) && strlen(p->propDescr)) {
                                        if (!strcasecmp(p->propName, prop->propName)) {
                                            count++;
                                        }
                                    }
                                    p = nextElement(&li);
                                }
                                /* If we find more than one */
                                if (count > 1) {
                                    free(cmp);
                                     #if DEBUG
                                        printf("Error! %s MUST appear once, yet it appeared %d times!\n", prop->propName, count);
                                    #endif
                                    return INV_EVENT;
                                }
                                found = 1;
                                free(cmp);
                                break;
                            }
                            free(cmp);
                        } else {
                            /* If it doesn't have to occur once, we can just say that
                            we found the very first occurance and move on */
                            if (!strcasecmp(eventprops[j], prop->propName)) {
                                found = 1;
                                break;
                            }
                        }
                    }
                    /* This property does not belong here */
                    if (!found) {
                        #if DEBUG
                            printf("Error!%s is not a valid event property!\n", prop->propName);
                        #endif
                        return INV_EVENT;
                    }
                }
                prop = nextElement(&propertyIterator);
            }
            /* if ( !hasDuration && !hasEnd ) {
                 return INV_EVENT;
            } */
        }

        /* If there exist some alarms, loop through them and verify */
        if (getLength(event->alarms)) {
            ListIterator alarmIterator = createIterator(event->alarms);
            Alarm * alarm = nextElement(&alarmIterator);

            while (alarm != NULL) {
                /* Action is empty */
                if ( !validLength(alarm->action, 200) || strcasecmp(alarm->action, "AUDIO") )
                    return INV_ALARM;

                /* Trigger is empty */
                if (alarm->trigger == NULL || alarm->trigger[0] == '\0'|| strlen(alarm->trigger) == 0)
                    return INV_ALARM;

                int duration = 0, repeat = 0;

                /* If the alarm contains some properties */
                if (getLength(alarm->properties)) {
                    ListIterator a_propertyIterator = createIterator(alarm->properties);
                    Property * a_prop = nextElement(&a_propertyIterator);

                    /* Loop through alarm properties */
                    while (a_prop != NULL) {
                        /* Checking for null & empty strings */
                        if (strlen(a_prop->propName) == 0 || a_prop->propDescr == NULL || strlen(a_prop->propDescr) == 0)
                            return INV_ALARM;
                        /* Duplicates */
                        if (!strcasecmp(a_prop->propName, "TRIGGER") || !strcasecmp(a_prop->propName, "ACTION"))
                            return INV_ALARM;
                        if (!strcasecmp(a_prop->propName, "DURATION")) {
                            if (duration) {
                                #if DEBUG
                                    printf("Duration occured more than once!\n");
                                #endif
                                return INV_ALARM;
                            }
                            duration++;
                        } else if (!strcasecmp(a_prop->propName, "REPEAT")) {
                            if (repeat) {
                                #if DEBUG
                                    printf("Repeat occured more than once!\n");
                                #endif
                                return INV_ALARM;
                            }
                            repeat++;
                        } else if (strcasecmp(a_prop->propName, "ATTACH")) {
                            /* If it's not an ATTACH */
                            #if DEBUG
                                printf("%s is not a valid audioprop\n", a_prop->propName);
                            #endif
                            return INV_ALARM;

                        }
                        a_prop = nextElement(&a_propertyIterator);
                    }
                }
                /* One can't appear without the other */
                if (!(!duration) ^ !(!repeat)) {
                    #if DEBUG
                        printf("Error! Duration can't appear without repeat && vice-versa!\n");
                    #endif
                    return INV_ALARM;
                }
                alarm = nextElement(&alarmIterator);
            }
        }
        #if DEBUG
            char * test = eventToJSON(event);
            printf("EVENT JSON:%s\n", test);
            free(test);
        #endif
        event = nextElement(&eventIterator);
    }
    #if DEBUG
        char * test = calendarToJSON(obj);
        printf("CAL JSON:%s\n", test);
        free(test);
    #endif
    /* After all testing is done */
    return OK;
}

/* unfolds */
char ** unfold (char ** file, int numLines, int * setLines ) {
    char ** result = NULL;
    int newLines = 0;

    for (int i = 0; i < numLines; i++) {
        int startIndex = 1;

        if (file[i][0] != ' ') {
            /* Allocate more space */
            if (result == NULL) result = (char **) calloc(1, sizeof(char *));
            else result = (char **) realloc(result, sizeof(char *) * (newLines + 1));
            /* Allocate a /0 */
            result[newLines] = (char *) calloc(1, 1);
            result[newLines][0] = '\0';
            newLines++;
            startIndex--;
        }

        /* Get length of current folded line */
        int len = strlen(file[i]) - 2;
        /* Allocate some temp string */
        char * line = (char *) calloc ( 1, len + startIndex + 1);
        /* Write temp string */
        for (int j = startIndex; j < len; j++)
            line[j - startIndex] = file[i][j];
        strcat(line, "\0");
        /* Reallocate some space in the array */
        if (result[newLines - 1] == NULL) result[newLines - 1] = (char *) calloc(1, strlen(line) + 1);
        else result[newLines - 1] = (char *) realloc(result[newLines - 1], strlen(result[newLines - 1]) + strlen(line) + 1);
        strcat(result[newLines - 1], line);
        /* Free temp */
        free(line);
    }

    /* Add line endings, I'm only doing this because my code was set up this way */
    for (int i = 0; i < newLines; i++) {
        result[i] = (char *) realloc(result[i], strlen(result[i]) + 3);
        strcat(result[i], "\r\n");
    }
    *setLines = newLines;
    return result;
}

/* MODULE 3 */
char* dtToJSON(DateTime prop) {
    /* {"date":"","time":"","isUTC":} */
    char * json = (char *) calloc(1, 30 + 9 + 7 + (prop.UTC ? 4 : 5) + 1);
    json[0] = '\0';
    /* We're done allocating */
    strcpy(json, "{\"date\":\"");
    strcat(json, prop.date);
    strcat(json, "\",\"time\":\"");
    strcat(json, prop.time);
    strcat(json, "\",\"isUTC\":");
    strcat(json, prop.UTC ? "true" : "false" );
    strcat(json, "}");
    /* Pheeww */
    return json;
}

char* eventToJSON(const Event* event) {
    /* If it's NULL */
    if (event == NULL) {
        char * json = (char *) calloc(1, 3);
        strcpy(json, "{}");
        return json;
    }
    /* {"startDT":DTval,"numProps":propVal,"numAlarms":almVal,"summary":"sumVal"} */
    char * json = (char *) calloc(1, 12);
    json[0] = '\0';
    strcpy(json, "{\"startDT\":");

    /* Add start date to the event */
    char * startDate = dtToJSON(event->startDateTime);
    json = (char *) realloc(json, strlen(json) + strlen(startDate) + 1);
    strcat(json, startDate);
    free(startDate);

    char * stampDate = dtToJSON(event->creationDateTime);
    json = (char *) realloc(json, strlen(json) + 20 + strlen(stampDate));
    strcat(json, ",\"stampDate\":");
    strcat(json, stampDate);
    free(stampDate);

    /* Move on to counting props */
    json = (char *) realloc(json, strlen(json) + 13);
    strcat(json, ",\"numProps\":");

    char numProps[10] = "";
    sprintf(numProps, "%d", 3 + getLength(event->properties));
    json = (char *) realloc(json, strlen(json) + strlen(numProps) + 1);
    strcat(json, numProps);

    /* We will now proceed to count alarms */
    json = (char *) realloc(json, strlen(json) + 14);
    strcat(json, ",\"numAlarms\":");

    char numAlarms[10] = "";
    sprintf(numAlarms, "%d", getLength(event->alarms));
    json = (char *) realloc(json, strlen(json) + strlen(numAlarms) + 1);
    strcat(json, numAlarms);

    json = (char *) realloc(json, strlen(json) + 15);
    strcat(json, ",\"summary\":\"");


    if (getLength(event->properties)) {
        ListIterator propertyIterator = createIterator(event->properties);
        Property * prop = nextElement(&propertyIterator);

        /* Loop through some properties */
        while (prop != NULL) {
            if (!strcasecmp(prop->propName, "SUMMARY")) {
                char * escpedSummary = escape(prop->propDescr);
                json = (char *) realloc(json, strlen(json) + strlen(escpedSummary) + 2 + 1);
                strcat(json, escpedSummary);
                free(escpedSummary);
                break;
            }
            prop = nextElement(&propertyIterator);
        }
    }

    char * props = propertyListToJSON(event->properties);

    json = (char *) realloc(json, strlen(json) + strlen(event->UID) + 20);
    strcat(json, "\",\"UID\":\"");
    strcat(json, event->UID);

    json = (char *) realloc(json, strlen(json) + 20 + strlen(props));
    strcat(json, "\",\"props\":");
    strcat(json, props);

    strcat(json, "}");
    free(props);
    return json;
}

char* alarmToJSON(const Alarm* alarm) {
    /* If it's NULL */
    if (alarm == NULL) {
        char * json = (char *) calloc(1, 3);
        strcpy(json, "{}");
        return json;
    }
    /* {"action":DTval,"trigger":propVal,"numProps":almVal} */
    char * json = (char *) calloc(1, 12);
    json[0] = '\0';
    strcpy(json, "{\"action\":");

    char * action = (char *) calloc(1, strlen(alarm->action) + 1);
    strcpy(action, alarm->action);

    json = realloc(json, strlen(json) + strlen(action) + 1 + 1 + 1 + 1);
    strcat(json, "\"");
    strcat(json, action);
    strcat(json, "\",");

    char * trigger = (char *) calloc(1, strlen(alarm->trigger) + 1);
    strcpy(trigger, alarm->trigger);
    json = realloc(json, strlen(json) + strlen(trigger) + 20 + 1);
    strcat(json, "\"trigger\":\"");
    strcat(json, trigger);
    strcat(json, "\",");

    int numProps = getLength(alarm->properties);
    char propStr[30] = "";
    sprintf(propStr, "\"numProps\":%d,", numProps);
    
    char * propList = propertyListToJSON(alarm->properties);

    json = realloc(json, strlen(json) + strlen(propStr) + 1 + strlen(propList) + 10);
    strcat(json, propStr);
    strcat(json, "\"props\":");
    strcat(json, propList);
    strcat(json, "}");

    /* Add start date to the event */
    
    free(action);
    free(trigger);
    free(propList);

    return json;
}

char* eventListToJSON(const List* el) {
    char * json = (char *) calloc(1, 2);
    strcpy(json, "[");

    /* This is kinda cheesy, but since I can't modify the header. I might as well .. */
    List l = * el;
    List * eventList = &l;

    /* Only if it's not NULL */
    if (eventList != NULL) {
        ListIterator eventIterator = createIterator(eventList);
        Event * event = nextElement(&eventIterator);
        int appendComma = 0;

        while (event != NULL) {
            if (appendComma) {
                json = (char *) realloc(json, strlen(json) + 2);
                strcat(json, ",");
            } else {
                appendComma = 1;
            }
            char * eventJSON = eventToJSON(event);
            json = (char *) realloc(json, strlen(json) + strlen(eventJSON) + 1);
            strcat(json, eventJSON);
            free(eventJSON);
            event = nextElement(&eventIterator);
        }
    }

    json = (char *) realloc(json, strlen(json) + 2);
    strcat(json, "]");
    return json;
}

char* alarmListToJSON(const List* al) {
    char * json = (char *) calloc(1, 2);
    strcpy(json, "[");

    List l = * al;
    List * alarmList = &l;

    if (alarmList != NULL) {
        ListIterator alarmIterator = createIterator(alarmList);
        Alarm * alarm = nextElement(&alarmIterator);
        int appendComma = 0;

        while (alarm != NULL) {
            if (appendComma) {
                json = (char *) realloc(json, strlen(json) + 2);
                strcat(json, ",");
            } else {
                appendComma = 1;
            }
            char * alarmJSON = alarmToJSON(alarm);
            json = (char *) realloc(json, strlen(json) + strlen(alarmJSON) + 1);
            strcat(json, alarmJSON);
            free(alarmJSON);
            alarm = nextElement(&alarmIterator);
        }
    }

    json = (char *) realloc(json, strlen(json) + 2);
    strcat(json, "]");
    return json;
}

char* alarmListToJSONWrapper(const Calendar * cal, int eventNumber) {
    List * eventList = cal->events;
    ListIterator eventIterator = createIterator(eventList);
    Event * event = nextElement(&eventIterator);
    int index = 1;

    while (event != NULL) {
        if (index == eventNumber) {
            return alarmListToJSON(event->alarms);
        }
        
        index++;
        event = nextElement(&eventIterator);
    }

    char * nothing = (char *) calloc(1, 3);
    strcpy(nothing, "[]");
    return nothing;
}

char* eventListToJSONWrapper(const Calendar* cal) {
    return eventListToJSON(cal->events);
}

char* calendarToJSON(const Calendar* cal) {
    char * json = (char *) calloc(1, 2);
    strcpy(json, "{");

    /*{"version":verVal,"prodID":"prodIDVal","numProps":propVal,"numEvents":evtVal}*/

    /* If it's not NULL */
    if (cal != NULL) {
        json = (char *) realloc(json, strlen(json) + 11);
        strcat(json, "\"version\":");

        /* Writing the version */
        char ver[10] = "";
        sprintf(ver, "%d", (int) cal->version);
        json = (char *) realloc(json, strlen(json) + strlen(ver) + 1);
        strcat(json, ver);

        /* Writing the PRODID */
        char * grabProdID = (char *) calloc(1, strlen(cal->prodID) + 1);
        strcpy(grabProdID, cal->prodID);
        char * escapedProdID = escape(grabProdID);
        json = (char *) realloc(json, strlen(json) + 11 + strlen(escapedProdID) + 1 + 1);
        strcat(json, ",\"prodID\":\"");
        strcat(json, escapedProdID);
        strcat(json, "\"");
        free(escapedProdID);
        free(grabProdID);

        json = (char *) realloc(json, strlen(json) + 13);
        strcat(json, ",\"numProps\":");
        char numProps[10] = "";
        sprintf(numProps, "%d", 2 + getLength(cal->properties));
        json = (char *) realloc(json, strlen(json) + strlen(numProps) + 1);
        strcat(json, numProps);

        json = (char *) realloc(json, strlen(json) + 14);
        strcat(json, ",\"numEvents\":");
        char numEvents[10] = "";
        sprintf(numEvents, "%d", getLength(cal->events));
        json = (char *) realloc(json, strlen(json) + strlen(numEvents) + 1);
        strcat(json, numEvents);
    }

    json = (char *) realloc(json, strlen(json) + 2);
    strcat(json, "}");
    return json;
}

char * propertyToJSON(const Property * prop) {
    int size = strlen(prop->propName) + strlen(prop->propDescr) + 35 + 2 + 2 + 1 + 1;
    char * json = (char *) calloc(1, size);
    strcpy(json, "{\"propName\":\"");
    strcat(json, prop->propName);
    strcat(json, "\",\"propDescr\":\"");
    strcat(json, prop->propDescr);
    strcat(json, "\"}");
    return json;
}

char * propertyListToJSON(const List * pl) {
    char * json = (char *) calloc(1, 2);
    strcpy(json, "[");

    List l = * pl;
    List * propList = &l;

    if (propList != NULL) {
        ListIterator propertyIterator = createIterator(propList);
        Property * prop = nextElement(&propertyIterator);
        int appendComma = 0;

        while (prop != NULL) {
            if (appendComma) {
                json = (char *) realloc(json, strlen(json) + 2);
                strcat(json, ",");
            } else {
                appendComma = 1;
            }
            char * propJSON = propertyToJSON(prop);
            json = (char *) realloc(json, strlen(json) + strlen(propJSON) + 1);
            strcat(json, propJSON);
            free(propJSON);
            prop = nextElement(&propertyIterator);
        }
    }

    json = (char *) realloc(json, strlen(json) + 2);
    strcat(json, "]");
    return json;
}

/* Converts a JSON string into a Calendar object */
Calendar* JSONtoCalendar(const char* str) {
    if (str == NULL || !strlen(str) || str[0] != '{' || str[strlen(str) - 1] != '}') return NULL;

    float version;
    /* Create a little object */
    Calendar * cal = (Calendar *) calloc(1, sizeof(Calendar) );
    cal->properties = initializeList(printProperty, deleteProperty, compareProperties);
    cal->events = initializeList(printEvent, deleteEvent, compareEvents);

    cal->prodID[0] = '\0';

    /* Let's look for version */
   // {"version":2,"prodID":"-//hacksw/handcal//NONSGML v1.0//EN"}
    int quote = 0;
    int foundProd = 0;
    int foundVer = 0;

    for (int i = 1; i < strlen(str) - 1; i++) {
        if (str[i] == '"') quote ^= 1;
        if (str[i] == ':' && !quote) {
            char propname[20] = "";
            int j, k;
            if (i - 8 >= 0) {
                for (j = i - 8, k = 0; j <= i - 2; j++, k++)
                    propname[k] = str[j];
                propname[++k] = '\0';

                if (!strcmp(propname, "version")) {
                    char ver[10] = "";
                    int j, k = 0;
                    for (j = i + 1; j < strlen(str); j++) {
                        if (str[j] != '.' && (str[j] < '0' || str[j] > '9')) break;
                        ver[k++] = str[j];
                    }
                    ver[k] = '\0';

                    if ( !strlen(ver) ) {
                        free(cal);
                        return NULL;
                    }
                    for (j = 0; j < strlen(ver); j++) {
                        if (ver[j] == '.') continue;
                        if (ver[j] < '0' || ver[j] > '9') {
                            free(cal);
                            return NULL;
                        }
                    }
                    version = atof(ver);
                    cal->version = version;
                    foundVer++;
                    #if DEBUG
                        printf("version found: %.2f\n", version);
                    #endif
                } else if ( !strcmp(propname, "\"prodID") ) {
                    int length = 0;
                    for (int j = i + 2; j < strlen(str) && str[j] != '"'; j++)
                        length++;
                    int k = 0;
                    for (int j = i + 2; j < i + 2 + length; j++)
                        cal->prodID[k++] = str[j];
                    cal->prodID[k] = '\0';
                    foundProd++;
                     #if DEBUG
                        printf("prodId: %s\n", cal->prodID);
                    #endif
                }
            }
        }
    }
    /* Something is broken, as the quote was not closed */
    if (quote || foundProd != 1 || foundVer != 1 || !strlen(cal->prodID)) {
        free(cal);
        return NULL;
    }

    /* Everything should be Gucci */
    return cal;
}

void JSONtoEventWrapper(Calendar * cal, char * json) {
    Event * e = JSONtoEvent(json);
    addEvent(cal, e);
}

/* Converts a JSON string into an Event */
Event * JSONtoEvent(const char* str) {
    if (str == NULL || !strlen(str) || str[0] != '{' || str[strlen(str) - 1] != '}') return NULL;

    Event * event = (Event *) calloc(1, sizeof(Event) );
    int length = strlen(str), quote = 0;

    event->properties = initializeList(printProperty, deleteProperty, compareProperties);
    event->alarms = initializeList(printAlarm, deleteAlarm, compareAlarms);

    int foundUID = 0;
    for (int i = 1; i < length - 1; i++) {
        if (str[i] == '"') quote ^= 1;
        if (str[i] == ':' && !quote) {
            int k = 0;
            /* make sure we actually have a UID and nothing else!!! */
            if (i - 5 > 0) {
                k = 0;
                if (str[i - 1] == '"' && str[i - 2] == 'D' && str[i - 3] == 'I' && str[i - 4] == 'U' && str[i - 5] == '"' && str[i + 1] == '"') {
                    for (int j = i + 2; j < strlen(str) && str[j] != '"'; j++)
                        event->UID[k++] = str[j];
                    event->UID[k] = '\0';

                    /* If it's still empty */
                    if (!strlen(event->UID)) {
                        free(event);
                        return NULL;
                    }
                    foundUID = 1;
                }
            } 
            
            if (i - 7 > 0) {
                k = 0;
                if (str[i-1] == '"' && str[i-2] == 'e' && str[i-3] == 't' && str[i-4] == 'a' && str[i-5] == 'd' && str[i-6] == 's' && str[i-7] == '"') {
                    for (int j = i + 2; j < strlen(str) && str[j] != '"'; j++)
                        event->startDateTime.date[k++] = str[j];
                    event->startDateTime.date[k] = '\0';
                } else if (str[i-1] == '"' && str[i-2] == 'e' && str[i-3] == 'm' && str[i-4] == 'i' && str[i-5] == 't' && str[i-6] == 's' && str[i-7] == '"') {
                    for (int j = i + 2; j < strlen(str) && str[j] != '"'; j++)
                        event->startDateTime.time[k++] = str[j];
                    event->startDateTime.time[k] = '\0';
                }
            } 
            
            if (i - 8 > 0) {
                k = 0;
                if (str[i-1] == '"' && str[i-2] == '2' && str[i-3] == 'e' && str[i-4] == 'm' && str[i-5] == 'i' && str[i-6] == 't' && str[i - 7] == 's' && str[i-8] == '"') {
                    for (int j = i + 2; j < strlen(str) && str[j] != '"'; j++)
                        event->creationDateTime.time[k++] = str[j];
                    event->creationDateTime.time[k] = '\0';
                } else if (str[i-1] == '"' && str[i-2] == '2' && str[i-3] == 'e' && str[i-4] == 't' && str[i-5] == 'a' && str[i-6] == 'd' && str[i - 7] == 's' && str[i-8] == '"') {
                    for (int j = i + 2; j < strlen(str) && str[j] != '"'; j++)
                        event->creationDateTime.date[k++] = str[j];
                    event->creationDateTime.date[k] = '\0';
                }
            }

        }
    }
    if (!foundUID || quote) {
        free(event);
        return NULL;
    }
    return event;
}

/* Inserts event into the calendar */
void addEvent(Calendar* cal, Event* toBeAdded) {
    /* Make sure the arguments exist & aren't NULL */
    if (cal != NULL && toBeAdded != NULL) {
        /* Create list if need be */
        if (cal->events == NULL)
            cal->events = initializeList(printEvent, deleteEvent, compareEvents);
        /* Insert event */
        insertBack(cal->events, toBeAdded);
    }
}

/* This function is used to escape quotes within a string */
char * escape (char * string ) {
    int len = strlen(string), index = 0;

    char * result = (char *) calloc(1, 1);
    result[0] = '\0';

    for (int i = 0; i < len; i++) {
        result = (char *) realloc(result, strlen(result) + (string[i] == '"' ? 2 : 1) + 1);
        if (string[i] == '"') {
            result[index] = '\\';
            result[index + 1] = '"';
            result[index + 2] = '\0';
            index += 2;
        } else {
            result[index] = string[i];
            result[index + 1] = '\0';
            index++;
        }
    }

    return result;
}