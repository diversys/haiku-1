/* 
 * Copyright 2002, Marcus Overhagen. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

#ifndef _DATA_EXCHANGE_H
#define _DATA_EXCHANGE_H

#include <MediaDefs.h>
#include <MediaNode.h>
#include <MediaAddOn.h>
#include <Entry.h>

namespace BPrivate {
namespace media {
namespace dataexchange {

struct reply_data;
struct request_data;

// BMessage based data exchange with the media_server
status_t SendToServer(BMessage *msg);
status_t QueryServer(BMessage *request, BMessage *reply);

// Raw data based data exchange with the media_server
status_t SendToServer(int32 msgcode, void *msg, int size);
status_t QueryServer(int32 msgcode, request_data *request, int requestsize, reply_data *reply, int replysize);

// Raw data based data exchange with the media_addon_server
status_t SendToAddonServer(int32 msgcode, void *msg, int size);
status_t QueryAddonServer(int32 msgcode, request_data *request, int requestsize, reply_data *reply, int replysize);

// Raw data based data exchange with the media_server
status_t SendToPort(port_id sendport, int32 msgcode, void *msg, int size);
status_t QueryPort(port_id requestport, int32 msgcode, request_data *request, int requestsize, reply_data *reply, int replysize);

// The base struct used for all raw requests
struct request_data
{
	port_id reply_port;

	void SendReply(status_t result, reply_data *reply, int replysize) const;
};

// The base struct used for all raw replys
struct reply_data
{
	status_t result;
};


}; // dataexchange
}; // media
}; // BPrivate

using namespace BPrivate::media::dataexchange;

// BMessage based server communication
enum {

	// BMediaRoster notification service
	MEDIA_SERVER_REQUEST_NOTIFICATIONS = 1000,
	MEDIA_SERVER_CANCEL_NOTIFICATIONS,
	MEDIA_SERVER_SEND_NOTIFICATIONS

};

// Raw port based communication
enum {
	SERVER_GET_NODE = 1000,
	SERVER_SET_NODE,
	CONSUMER_ACCEPT_FORMAT,
	CONSUMER_CONNECTED,
	PRODUCER_FORMAT_PROPOSAL,
	PRODUCER_PREPARE_TO_CONNECT,
	PRODUCER_CONNECT,
};

// used by SERVER_GET_NODE and SERVER_SET_NODE
enum node_type 
{ 
	VIDEO_INPUT, 
	AUDIO_INPUT, 
	VIDEO_OUTPUT, 
	AUDIO_MIXER, 
	AUDIO_OUTPUT, 
	AUDIO_OUTPUT_EX, 
	TIME_SOURCE, 
	SYSTEM_TIME_SOURCE 
};


struct addonserver_instantiate_dormant_node_request : public request_data
{
	dormant_node_info info;
};

struct addonserver_instantiate_dormant_node_reply : public reply_data
{
	media_node node;
};

struct server_set_node_request : public request_data
{
	node_type type;
	bool use_node;
	media_node node;
	bool use_dni;
	dormant_node_info dni;
	bool use_input;
	media_input input;
};

struct server_set_node_reply : public reply_data
{
};

struct server_get_node_request : public request_data
{
	node_type type;
};

struct server_get_node_reply : public reply_data
{
	media_node node;

	// for AUDIO_OUTPUT_EX
	char input_name[B_MEDIA_NAME_LENGTH];
	int32 input_id;
};

struct producer_format_proposal_request : public request_data
{
	media_source output;
	media_format format;
};

struct producer_format_proposal_reply : public reply_data
{
	media_format format;
};

struct producer_prepare_to_connect_request : public request_data
{
	media_source source;
	media_destination destination;
	media_format format;
	char name[B_MEDIA_NAME_LENGTH];
};

struct producer_prepare_to_connect_reply : public reply_data
{
	media_format format;
	media_source out_source;
	char name[B_MEDIA_NAME_LENGTH];
};

struct producer_connect_request : public request_data
{
	status_t error;
	media_source source;
	media_destination destination;
	media_format format;
	char name[B_MEDIA_NAME_LENGTH];
};

struct producer_connect_reply : public reply_data
{
	char name[B_MEDIA_NAME_LENGTH];
};

struct consumer_accept_format_request : public request_data
{
	media_destination dest;
	media_format format;
};

struct consumer_accept_format_reply : public reply_data
{
	media_format format;
};

struct consumer_connected_request : public request_data
{
	media_source producer;
	media_destination where;
	media_format with_format;
};

struct consumer_connected_reply : public reply_data
{
	media_input input;
};

#endif // _DATA_EXCHANGE_H
