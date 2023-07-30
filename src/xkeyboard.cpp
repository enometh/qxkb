/***************************************************************************
                          xkeyboard.cpp  -  description
                             -------------------
    begin                : Sun Jul 8 2001
    copyright            : (C) 2001 by Leonid Zeitlin
    email                : lz@europe.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "xkeyboard.h"

#define explicit explicit_
#include <xcb/xcb.h>
#include <xcb/xkb.h>

XKeyboard *XKeyboard::m_self = 0;

XKeyboard::XKeyboard()
{

	xcb_connection_t *conn =  QX11Info::connection();
	if (! conn)
		qCritical()<<"Could not obtain an XCBConnection.\n";

	// check the library version
	const xcb_query_extension_reply_t *extreply;
	extreply = xcb_get_extension_data(conn, &xcb_xkb_id);
	if (!extreply->present) {
		qCritical() << ("The X Server does not support a compatible XKB extension.\n"
		                  "Either the server is not XKB-capable or the extension was disabled.\n"
		                  "This program would not work with this server, so it will exit now\n");
	} else {
		qDebug()<<"initializing xcb-xkb\n";
		m_event_code = extreply->first_event;
		m_error_code = extreply->first_error;
		xcb_xkb_use_extension(conn, XCB_XKB_MAJOR_VERSION, XCB_XKB_MINOR_VERSION);

		static const xcb_xkb_map_part_t required_map_parts =
			(xcb_xkb_map_part_t)
			(XCB_XKB_MAP_PART_KEY_TYPES |
			 XCB_XKB_MAP_PART_KEY_SYMS |
			 XCB_XKB_MAP_PART_MODIFIER_MAP |
			 XCB_XKB_MAP_PART_EXPLICIT_COMPONENTS |
			 XCB_XKB_MAP_PART_KEY_ACTIONS |
			 XCB_XKB_MAP_PART_VIRTUAL_MODS |
			 XCB_XKB_MAP_PART_VIRTUAL_MOD_MAP);

		static const xcb_xkb_event_type_t required_events =
			(xcb_xkb_event_type_t)
			(//// new keyboard:
				XCB_XKB_EVENT_TYPE_NEW_KEYBOARD_NOTIFY |
				//// group names change:
				XCB_XKB_EVENT_TYPE_NAMES_NOTIFY |
				//// keyboard mapping change:
				XCB_XKB_EVENT_TYPE_MAP_NOTIFY |
				//// group state change, i.e. the current group changed:
				XCB_XKB_EVENT_TYPE_STATE_NOTIFY //|
//				XCB_XKB_EVENT_TYPE_INDICATOR_STATE_NOTIFY |
//				XCB_XKB_EVENT_TYPE_MAP_NOTIFY
				);

		// register for XKB events
		xcb_xkb_select_events(conn,
				      // deviceSpec
				      XCB_XKB_ID_USE_CORE_KBD,
				      // affectWhich
				      required_events,
				      //clear
				      0,
				      //selectAll
				      required_events,
				      //affectMap
				      0xff,
				      //map
				      0xff,
				      //details
				      NULL);

		retrieveNumKbdGroups();
	}
	m_self = this;

}

XKeyboard::~XKeyboard()
{
	qDebug()<<"XKeyboard destructor";
}

/** Set the current keyboard group to the given groupno */
void XKeyboard::setGroupNo(int groupno)
{
	xcb_connection_t *conn =  QX11Info::connection();
	xcb_xkb_latch_lock_state(conn,
				 XCB_XKB_ID_USE_CORE_KBD,
				 0, // affectModLocks
				 0, // modLocks
				 True, // lockGroup
				 groupno, // groupLock
				 0,    // affectModLatches
				 False, // latchGroup
				 0);	// GroupLatch
}

#ifndef HAVE_LIBXKLAVIER
extern "C" {
	static int IgnoreXError(Display *, XErrorEvent *)
	{
		return 0;
	}
}
#endif

XKeyboard * XKeyboard::self()
{
	return m_self;
}

/** return the current keyboard group index */
int XKeyboard::getGroupNo()
{
	int group = -1;
	xcb_connection_t *conn =  QX11Info::connection();
	xcb_generic_error_t *e;
	xcb_xkb_get_state_reply_t *rec =
		xcb_xkb_get_state_reply(conn,
			xcb_xkb_get_state(conn, XCB_XKB_ID_USE_CORE_KBD),
					&e);
	if (rec) {
		group = rec->group;
		free(rec);
	} else if (e) {
		qCritical()<<"xkb_get_state failed on " << e->resource_id << " with error code " << e->error_code;
		free(e);
	}
	return group;
}



void XKeyboard::retrieveNumKbdGroups()
{
	xcb_connection_t *conn =  QX11Info::connection();
	xcb_generic_error_t *e;
	xcb_xkb_get_controls_reply_t *ctrls =
		xcb_xkb_get_controls_reply(conn,
		   xcb_xkb_get_controls(conn, XCB_XKB_ID_USE_CORE_KBD),
					   &e);
	if (ctrls) {
		m_numgroups = ctrls->numGroups;
	} else if (e) {
		qCritical()<<"xkb_get_controls failed on " << e->resource_id << " with error code " << e->error_code;
		free(e);
	}
	free(ctrls);
}


#if 0
/** Examines an X Event passed to it and takes actions if the event is of
  * interest to XKeyboard */
void XKeyboard::processEvent(XEvent *ev)
{
	if (ev->type == m_event_code) {
		// This an XKB event
		XkbEvent *xkb_ev = (XkbEvent *) ev;
		if (xkb_ev->any.xkb_type == XkbStateNotify) {
			// state notify event, the current group has changed
			emit groupChanged(xkb_ev->state.group);
		} else if (((xkb_ev->any.xkb_type == XkbMapNotify) && (xkb_ev->map.changed & XkbKeySymsMask))
		           || ((xkb_ev->any.xkb_type == XkbNamesNotify) && (xkb_ev->names.changed & XkbGroupNamesMask))
		           || (xkb_ev->any.xkb_type == XkbNewKeyboardNotify)) {
			// keyboard layout has changed
			retrieveNumKbdGroups();
			emit layoutChanged();
		}

	}
}
#endif
// 22, 85

bool
XKeyboard::nativeEventFilter(const QByteArray &eventType, void *message, long *result)
{
	qDebug()<<"QXKB::nativeEventFilter";
        if (eventType == QByteArrayLiteral("xcb_generic_event_t")) {
		const xcb_generic_event_t *e = (xcb_generic_event_t *)(message);
		if (e->response_type & ~0x80 != m_event_code)
			return false;
		// there isn't any common xkb_event struct, but we
		// want to get the second xkbType uint8 field which is
		// in all xkb_event_* structs
		auto any_xkb_type = ((xcb_xkb_state_notify_event_t *)e)->xkbType;
		if (any_xkb_type == XCB_XKB_EVENT_TYPE_STATE_NOTIFY) {
			// state notify event, the current group has changed
			emit groupChanged(((xcb_xkb_state_notify_event_t *)e)->group);
			qDebug()<<"Emit groupChanged";
		} else if (((any_xkb_type == XCB_XKB_EVENT_TYPE_MAP_NOTIFY) &&
			    (((xcb_xkb_indicator_map_notify_event_t *)e)->mapChanged))
		           || (any_xkb_type == XCB_XKB_EVENT_TYPE_NAMES_NOTIFY) && (((xcb_xkb_names_notify_event_t *)e)->changed)
			   || (any_xkb_type == XCB_XKB_EVENT_TYPE_NEW_KEYBOARD_NOTIFY)) {
			// keyboard layout has changed
			retrieveNumKbdGroups();
			emit layoutChanged();
			qDebug()<<"Emit layoutChanged";
		}
		return true;
 	}
	return false;
}
