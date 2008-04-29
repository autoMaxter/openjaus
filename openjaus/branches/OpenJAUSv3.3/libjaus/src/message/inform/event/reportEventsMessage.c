/*****************************************************************************
 *  Copyright (c) 2008, OpenJAUS.com
 *  All rights reserved.
 *  
 *  This file is part of OpenJAUS.  OpenJAUS is distributed under the BSD 
 *  license.  See the LICENSE file for details.
 * 
 *  Redistribution and use in source and binary forms, with or without 
 *  modification, are permitted provided that the following conditions 
 *  are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of the University of Florida nor the names of its 
 *       contributors may be used to endorse or promote products derived from 
 *       this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ****************************************************************************/
// File Name: reportEventsMessage.c
//
// Written By: Danny Kent (jaus AT dannykent DOT com), Tom Galluzzo (galluzzo AT gmail DOT com)
//
// Version: 3.3 BETA
//
// Date: 04/15/08
//
// Description: This file defines the functionality of a ReportEventsMessage



#include <stdlib.h>
#include <string.h>
#include "jaus.h"

static const int commandCode = JAUS_REPORT_EVENTS;
static const int maxDataSizeBytes = 512000; // Max Message size: 500K

static JausBoolean headerFromBuffer(ReportEventsMessage message, unsigned char *buffer, unsigned int bufferSizeBytes);
static JausBoolean headerToBuffer(ReportEventsMessage message, unsigned char *buffer, unsigned int bufferSizeBytes);

static JausBoolean dataFromBuffer(ReportEventsMessage message, unsigned char *buffer, unsigned int bufferSizeBytes);
static int dataToBuffer(ReportEventsMessage message, unsigned char *buffer, unsigned int bufferSizeBytes);
static void dataInitialize(ReportEventsMessage message);
static void dataDestroy(ReportEventsMessage message);
static unsigned int dataSize(ReportEventsMessage message);

// ************************************************************************************************************** //
//                                    USER CONFIGURED FUNCTIONS
// ************************************************************************************************************** //

// Initializes the message-specific fields
static void dataInitialize(ReportEventsMessage message)
{
	// Set initial values of message fields

	message->count = newJausByte(0);
	message->presenceVector = NULL;
	message->messageCode = NULL;
	message->eventType = NULL;
	message->eventBoundary = NULL;
	message->limitDataField = NULL;
	message->lowerLimit = NULL;
	message->upperLimit = NULL;
	message->stateLimit = NULL;
	message->eventId = NULL;
	message->queryMessage = NULL;	
}

// Destructs the message-specific fields
static void dataDestroy(ReportEventsMessage message)
{
	int i;
	
	for(i=0; i<message->count; i++)
	{
		// Free message fields
		if(message->presenceVector) free(message->presenceVector);
		if(message->messageCode) free(message->messageCode);
		if(message->eventType) free(message->eventType);
		if(message->eventBoundary) free(message->eventBoundary);		
		
		if(message->lowerLimit && message->lowerLimit[i])
		{
			jausEventLimitDestroy(message->lowerLimit[i]);
		}

		if(message->upperLimit && message->upperLimit[i])
		{
			jausEventLimitDestroy(message->upperLimit[i]);
		}

		if(message->stateLimit && message->stateLimit[i])
		{
			jausEventLimitDestroy(message->stateLimit[i]);
		}

		if(message->eventId) free(message->eventId);

		if(message->queryMessage && message->queryMessage[i])
		{
			jausMessageDestroy(message->queryMessage[i]);
		}
	}
}

// Return boolean of success
static JausBoolean dataFromBuffer(ReportEventsMessage message, unsigned char *buffer, unsigned int bufferSizeBytes)
{
	int i;
	int index = 0;
	
	if(bufferSizeBytes == message->dataSize)
	{
		// Unpack Message Fields from Buffer
		// Event Count
		if(!jausByteFromBuffer(&message->count, buffer+index, bufferSizeBytes-index)) return JAUS_FALSE;
		index += JAUS_BYTE_SIZE_BYTES;

		message->presenceVector = (JausByte *)malloc(message->count * sizeof(JausByte));
		message->messageCode = (JausUnsignedShort *)malloc(message->count * sizeof(JausUnsignedShort));
		message->eventType = (JausByte *)malloc(message->count * sizeof(JausByte));
		message->eventBoundary = (JausByte *)malloc(message->count * sizeof(JausByte));
		message->limitDataField = (JausByte *)malloc(message->count * sizeof(JausByte));
		message->lowerLimit = (JausEventLimit *)malloc(message->count * sizeof(JausEventLimit));
		message->upperLimit = (JausEventLimit *)malloc(message->count * sizeof(JausEventLimit));
		message->stateLimit = (JausEventLimit *)malloc(message->count * sizeof(JausEventLimit));
		message->eventId = (JausByte *)malloc(message->count * sizeof(JausByte));
		message->queryMessage = (JausMessage *)malloc(message->count * sizeof(JausMessage));
		
		for(i=0; i<message->count; i++)
		{
			if(!jausByteFromBuffer(&message->presenceVector[i], buffer+index, bufferSizeBytes-index)) return JAUS_FALSE;
			index += JAUS_BYTE_SIZE_BYTES;
		
			// Message Code
			if(!jausUnsignedShortFromBuffer(&message->messageCode[i], buffer+index, bufferSizeBytes-index)) return JAUS_FALSE;
			index += JAUS_UNSIGNED_SHORT_SIZE_BYTES;		
	
			// Event Type
			if(!jausByteFromBuffer(&message->eventType[i], buffer+index, bufferSizeBytes-index)) return JAUS_FALSE;
			index += JAUS_BYTE_SIZE_BYTES;
			
			if(jausByteIsBitSet(message->presenceVector[i], REPORT_EVENTS_PV_BOUNDARY_BIT))
			{
				// Event Boundary
				if(!jausByteFromBuffer(&message->eventBoundary[i], buffer+index, bufferSizeBytes-index)) return JAUS_FALSE;
				index += JAUS_BYTE_SIZE_BYTES;
			}
	
			if(jausByteIsBitSet(message->presenceVector[i], REPORT_EVENTS_PV_DATA_FIELD_BIT))
			{
				// Data Field
				if(!jausByteFromBuffer(&message->limitDataField[i], buffer+index, bufferSizeBytes-index)) return JAUS_FALSE;
				index += JAUS_BYTE_SIZE_BYTES;
			}
			
			if(jausByteIsBitSet(message->presenceVector[i], REPORT_EVENTS_PV_LOWER_LIMIT_BIT))
			{		
				// Lower Limit
				if(!jausEventLimitFromBuffer(&message->lowerLimit[i], buffer+index, bufferSizeBytes-index)) return JAUS_FALSE;
				index += jausEventLimitSize(message->lowerLimit[i]);
			}
			else
			{
				message->lowerLimit[i] = NULL;
			}
			
			if(jausByteIsBitSet(message->presenceVector[i], REPORT_EVENTS_PV_UPPER_LIMIT_BIT))
			{		
				// Upper Limit
				if(!jausEventLimitFromBuffer(&message->upperLimit[i], buffer+index, bufferSizeBytes-index)) return JAUS_FALSE;
				index += jausEventLimitSize(message->upperLimit[i]);
			}
			else
			{
				message->upperLimit[i] = NULL;
			}
	
			if(jausByteIsBitSet(message->presenceVector[i], REPORT_EVENTS_PV_STATE_LIMIT_BIT))
			{		
				// State Limit
				if(!jausEventLimitFromBuffer(&message->stateLimit[i], buffer+index, bufferSizeBytes-index)) return JAUS_FALSE;
				index += jausEventLimitSize(message->stateLimit[i]);
			}
			else
			{
				message->stateLimit[i] = NULL;
			}
			
			
			if(jausByteIsBitSet(message->presenceVector[i], REPORT_EVENTS_PV_EVENT_ID_BIT))
			{		
				if(!jausByteFromBuffer(&message->eventId[i], buffer+index, bufferSizeBytes-index)) return JAUS_FALSE;
				index += JAUS_BYTE_SIZE_BYTES;		
			}
			
			if(jausByteIsBitSet(message->presenceVector[i], REPORT_EVENTS_PV_QUERY_MESSAGE_BIT))
			{		
				index += JAUS_UNSIGNED_INTEGER_SIZE_BYTES;
				
				message->queryMessage[i] = jausMessageCreate();
				
				// Jaus Message
				if(!jausMessageFromBuffer(message->queryMessage[i], buffer+index, bufferSizeBytes-index)) return JAUS_FALSE;
				index += jausMessageSize(message->queryMessage[i]);
			}
			
		}// for i<message->count

		return JAUS_TRUE;
	}
	else
	{
		return JAUS_FALSE;
	}
}

// Returns number of bytes put into the buffer
static int dataToBuffer(ReportEventsMessage message, unsigned char *buffer, unsigned int bufferSizeBytes)
{
	int i;
	int index = 0;

	if(bufferSizeBytes >= dataSize(message))
	{
		// Pack Message Fields to Buffer
		// Message Count
		if(!jausByteToBuffer(message->count, buffer+index, bufferSizeBytes-index)) return JAUS_FALSE;
		index += JAUS_BYTE_SIZE_BYTES;

		for(i=0; i<message->count; i++)
		{
			// Presence Vector
			if(!jausByteToBuffer(message->presenceVector[i], buffer+index, bufferSizeBytes-index)) return JAUS_FALSE;
			index += JAUS_BYTE_SIZE_BYTES;
			
			// Message Code
			if(!jausUnsignedShortToBuffer(message->messageCode[i], buffer+index, bufferSizeBytes-index)) return JAUS_FALSE;
			index += JAUS_UNSIGNED_SHORT_SIZE_BYTES;
			
			// Event Type
			if(!jausByteToBuffer(message->eventType[i], buffer+index, bufferSizeBytes-index)) return JAUS_FALSE;
			index += JAUS_BYTE_SIZE_BYTES;
			
			if(jausByteIsBitSet(message->presenceVector[i], REPORT_EVENTS_PV_BOUNDARY_BIT))
			{
				// Event Boundary
				if(!jausByteToBuffer(message->eventBoundary[i], buffer+index, bufferSizeBytes-index)) return JAUS_FALSE;
				index += JAUS_BYTE_SIZE_BYTES;
			}
	
			if(jausByteIsBitSet(message->presenceVector[i], REPORT_EVENTS_PV_DATA_FIELD_BIT))
			{
				// Data Field
				if(!jausByteToBuffer(message->limitDataField[i], buffer+index, bufferSizeBytes-index)) return JAUS_FALSE;
				index += JAUS_BYTE_SIZE_BYTES;
			}
			
			if(jausByteIsBitSet(message->presenceVector[i], REPORT_EVENTS_PV_LOWER_LIMIT_BIT))
			{		
				// Lower Limit
				if(!jausEventLimitToBuffer(message->lowerLimit[i], buffer+index, bufferSizeBytes-index)) return JAUS_FALSE;
				index += jausEventLimitSize(message->lowerLimit[i]);
			}
			
			if(jausByteIsBitSet(message->presenceVector[i], REPORT_EVENTS_PV_UPPER_LIMIT_BIT))
			{		
				// Upper Limit
				if(!jausEventLimitToBuffer(message->upperLimit[i], buffer+index, bufferSizeBytes-index)) return JAUS_FALSE;
				index += jausEventLimitSize(message->upperLimit[i]);
			}
	
			if(jausByteIsBitSet(message->presenceVector[i], REPORT_EVENTS_PV_STATE_LIMIT_BIT))
			{		
				// State Limit
				if(!jausEventLimitToBuffer(message->stateLimit[i], buffer+index, bufferSizeBytes-index)) return JAUS_FALSE;
				index += jausEventLimitSize(message->stateLimit[i]);
			}
					
			if(jausByteIsBitSet(message->presenceVector[i], REPORT_EVENTS_PV_EVENT_ID_BIT))
			{		
				if(!jausByteToBuffer(message->eventId[i], buffer+index, bufferSizeBytes-index)) return JAUS_FALSE;
				index += JAUS_BYTE_SIZE_BYTES;
			}
				
			if(jausByteIsBitSet(message->presenceVector[i], REPORT_EVENTS_PV_QUERY_MESSAGE_BIT))
			{		
				if(!jausUnsignedIntegerToBuffer((JausUnsignedInteger)jausMessageSize(message->queryMessage[i]), buffer+index, bufferSizeBytes-index)) return JAUS_FALSE;
				index += JAUS_UNSIGNED_INTEGER_SIZE_BYTES;
	
				// Jaus Message
				if(!jausMessageToBuffer(message->queryMessage[i], buffer+index, bufferSizeBytes)) return JAUS_FALSE;
				index += jausMessageSize(message->queryMessage[i]);
			}
		} // for i<message->count
	}

	return index;
}

// Returns number of bytes put into the buffer
static unsigned int dataSize(ReportEventsMessage message)
{
	int i;
	int index = 0;

	// Message Count
	index += JAUS_BYTE_SIZE_BYTES;

	// Presence Vectors
	index += message->count * JAUS_BYTE_SIZE_BYTES;

	// Message Code
	index += message->count * JAUS_UNSIGNED_SHORT_SIZE_BYTES;
	
	// Event Type
	index += message->count * JAUS_BYTE_SIZE_BYTES;
	
	for(i=0; i<message->count; i++)
	{
		if(jausByteIsBitSet(message->presenceVector[i], REPORT_EVENTS_PV_BOUNDARY_BIT))
		{
			index += JAUS_BYTE_SIZE_BYTES;
		}
	
		if(jausByteIsBitSet(message->presenceVector[i], REPORT_EVENTS_PV_DATA_FIELD_BIT))
		{
			index += JAUS_BYTE_SIZE_BYTES;
		}
		
		if(jausByteIsBitSet(message->presenceVector[i], REPORT_EVENTS_PV_LOWER_LIMIT_BIT))
		{		
			index += jausEventLimitSize(message->lowerLimit[i]);
		}
		
		if(jausByteIsBitSet(message->presenceVector[i], REPORT_EVENTS_PV_UPPER_LIMIT_BIT))
		{		
			index += jausEventLimitSize(message->upperLimit[i]);
		}
	
		if(jausByteIsBitSet(message->presenceVector[i], REPORT_EVENTS_PV_STATE_LIMIT_BIT))
		{		
			index += jausEventLimitSize(message->stateLimit[i]);
		}
		
		if(jausByteIsBitSet(message->presenceVector[i], REPORT_EVENTS_PV_EVENT_ID_BIT))
		{		
			index += JAUS_BYTE_SIZE_BYTES;
		}
		
		if(jausByteIsBitSet(message->presenceVector[i], REPORT_EVENTS_PV_QUERY_MESSAGE_BIT))
		{		
			index += JAUS_UNSIGNED_INTEGER_SIZE_BYTES;
			// Jaus Message
			index += jausMessageSize(message->queryMessage[i]);
		}
	} // end for loop

	return index;
}


// ************************************************************************************************************** //
//                                    NON-USER CONFIGURED FUNCTIONS
// ************************************************************************************************************** //

ReportEventsMessage reportEventsMessageCreate(void)
{
	ReportEventsMessage message;

	message = (ReportEventsMessage)malloc( sizeof(ReportEventsMessageStruct) );
	if(message == NULL)
	{
		return NULL;
	}
	
	// Initialize Values
	message->properties.priority = JAUS_DEFAULT_PRIORITY;
	message->properties.ackNak = JAUS_ACK_NAK_NOT_REQUIRED;
	message->properties.scFlag = JAUS_NOT_SERVICE_CONNECTION_MESSAGE;
	message->properties.expFlag = JAUS_NOT_EXPERIMENTAL_MESSAGE;
	message->properties.version = JAUS_VERSION_3_3;
	message->properties.reserved = 0;
	message->commandCode = commandCode;
	message->destination = jausAddressCreate();
	message->source = jausAddressCreate();
	message->dataFlag = JAUS_SINGLE_DATA_PACKET;
	message->dataSize = maxDataSizeBytes;
	message->sequenceNumber = 0;
	
	dataInitialize(message);
	message->dataSize = dataSize(message);
	
	return message;	
}

void reportEventsMessageDestroy(ReportEventsMessage message)
{
	dataDestroy(message);
	jausAddressDestroy(message->source);
	jausAddressDestroy(message->destination);
	free(message);
	message = NULL;
}

JausBoolean reportEventsMessageFromBuffer(ReportEventsMessage message, unsigned char* buffer, unsigned int bufferSizeBytes)
{
	int index = 0;
	
	if(headerFromBuffer(message, buffer+index, bufferSizeBytes-index))
	{
		index += JAUS_HEADER_SIZE_BYTES;
		if(dataFromBuffer(message, buffer+index, bufferSizeBytes-index))
		{
			return JAUS_TRUE;
		}
		else
		{
			return JAUS_FALSE;
		}
	}
	else
	{
		return JAUS_FALSE;
	}
}

JausBoolean reportEventsMessageToBuffer(ReportEventsMessage message, unsigned char *buffer, unsigned int bufferSizeBytes)
{
	if(bufferSizeBytes < reportEventsMessageSize(message))
	{
		return JAUS_FALSE; //improper size	
	}
	else
	{	
		message->dataSize = dataToBuffer(message, buffer+JAUS_HEADER_SIZE_BYTES, bufferSizeBytes - JAUS_HEADER_SIZE_BYTES);
		if(headerToBuffer(message, buffer, bufferSizeBytes))
		{
			return JAUS_TRUE;
		}
		else
		{
			return JAUS_FALSE; // headerToReportEventsBuffer failed
		}
	}
}

ReportEventsMessage reportEventsMessageFromJausMessage(JausMessage jausMessage)
{
	ReportEventsMessage message;

	if(jausMessage->commandCode != commandCode)
	{
		return NULL; // Wrong message type
	}
	else
	{
		message = (ReportEventsMessage)malloc( sizeof(ReportEventsMessageStruct) );
		if(message == NULL)
		{
			return NULL;
		}
		
		message->properties.priority = jausMessage->properties.priority;
		message->properties.ackNak = jausMessage->properties.ackNak;
		message->properties.scFlag = jausMessage->properties.scFlag;
		message->properties.expFlag = jausMessage->properties.expFlag;
		message->properties.version = jausMessage->properties.version;
		message->properties.reserved = jausMessage->properties.reserved;
		message->commandCode = jausMessage->commandCode;
		message->destination = jausAddressCreate();
		*message->destination = *jausMessage->destination;
		message->source = jausAddressCreate();
		*message->source = *jausMessage->source;
		message->dataSize = jausMessage->dataSize;
		message->dataFlag = jausMessage->dataFlag;
		message->sequenceNumber = jausMessage->sequenceNumber;
		
		// Unpack jausMessage->data
		if(dataFromBuffer(message, jausMessage->data, jausMessage->dataSize))
		{
			return message;
		}
		else
		{
			return NULL;
		}
	}
}

JausMessage reportEventsMessageToJausMessage(ReportEventsMessage message)
{
	JausMessage jausMessage;
	
	jausMessage = (JausMessage)malloc( sizeof(struct JausMessageStruct) );
	if(jausMessage == NULL)
	{
		return NULL;
	}	
	
	jausMessage->properties.priority = message->properties.priority;
	jausMessage->properties.ackNak = message->properties.ackNak;
	jausMessage->properties.scFlag = message->properties.scFlag;
	jausMessage->properties.expFlag = message->properties.expFlag;
	jausMessage->properties.version = message->properties.version;
	jausMessage->properties.reserved = message->properties.reserved;
	jausMessage->commandCode = message->commandCode;
	jausMessage->destination = jausAddressCreate();
	*jausMessage->destination = *message->destination;
	jausMessage->source = jausAddressCreate();
	*jausMessage->source = *message->source;
	jausMessage->dataSize = dataSize(message);
	jausMessage->dataFlag = message->dataFlag;
	jausMessage->sequenceNumber = message->sequenceNumber;
	
	jausMessage->data = (unsigned char *)malloc(jausMessage->dataSize);
	jausMessage->dataSize = dataToBuffer(message, jausMessage->data, jausMessage->dataSize);
	
	return jausMessage;
}


unsigned int reportEventsMessageSize(ReportEventsMessage message)
{
	return (unsigned int)(dataSize(message) + JAUS_HEADER_SIZE_BYTES);
}

//********************* PRIVATE HEADER FUNCTIONS **********************//

static JausBoolean headerFromBuffer(ReportEventsMessage message, unsigned char *buffer, unsigned int bufferSizeBytes)
{
	if(bufferSizeBytes < JAUS_HEADER_SIZE_BYTES)
	{ 
		return JAUS_FALSE;
	}
	else
	{
		// unpack header
		message->properties.priority = (buffer[0] & 0x0F);
		message->properties.ackNak	 = ((buffer[0] >> 4) & 0x03);
		message->properties.scFlag	 = ((buffer[0] >> 6) & 0x01);
		message->properties.expFlag	 = ((buffer[0] >> 7) & 0x01);
		message->properties.version	 = (buffer[1] & 0x3F);
		message->properties.reserved = ((buffer[1] >> 6) & 0x03);
		
		message->commandCode = buffer[2] + (buffer[3] << 8);
	
		message->destination->instance = buffer[4];
		message->destination->component = buffer[5];
		message->destination->node = buffer[6];
		message->destination->subsystem = buffer[7];
	
		message->source->instance = buffer[8];
		message->source->component = buffer[9];
		message->source->node = buffer[10];
		message->source->subsystem = buffer[11];
		
		message->dataSize = buffer[12] + ((buffer[13] & 0x0F) << 8);

		message->dataFlag = ((buffer[13] >> 4) & 0x0F);

		message->sequenceNumber = buffer[14] + (buffer[15] << 8);
		
		return JAUS_TRUE;
	}
}

static JausBoolean headerToBuffer(ReportEventsMessage message, unsigned char *buffer, unsigned int bufferSizeBytes)
{
	JausUnsignedShort *propertiesPtr = (JausUnsignedShort*)&message->properties;
	
	if(bufferSizeBytes < JAUS_HEADER_SIZE_BYTES)
	{
		return JAUS_FALSE;
	}
	else
	{	
		buffer[0] = (unsigned char)(*propertiesPtr & 0xFF);
		buffer[1] = (unsigned char)((*propertiesPtr & 0xFF00) >> 8);

		buffer[2] = (unsigned char)(message->commandCode & 0xFF);
		buffer[3] = (unsigned char)((message->commandCode & 0xFF00) >> 8);

		buffer[4] = (unsigned char)(message->destination->instance & 0xFF);
		buffer[5] = (unsigned char)(message->destination->component & 0xFF);
		buffer[6] = (unsigned char)(message->destination->node & 0xFF);
		buffer[7] = (unsigned char)(message->destination->subsystem & 0xFF);

		buffer[8] = (unsigned char)(message->source->instance & 0xFF);
		buffer[9] = (unsigned char)(message->source->component & 0xFF);
		buffer[10] = (unsigned char)(message->source->node & 0xFF);
		buffer[11] = (unsigned char)(message->source->subsystem & 0xFF);
		
		buffer[12] = (unsigned char)(message->dataSize & 0xFF);
		buffer[13] = (unsigned char)((message->dataFlag & 0xFF) << 4) | (unsigned char)((message->dataSize & 0x0F00) >> 8);

		buffer[14] = (unsigned char)(message->sequenceNumber & 0xFF);
		buffer[15] = (unsigned char)((message->sequenceNumber & 0xFF00) >> 8);
		
		return JAUS_TRUE;
	}
}

