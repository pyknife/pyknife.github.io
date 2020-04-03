/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stddef.h>
#include "platform.h"	// For timestamp
#include "tcpm.h"
#include "pd.h"

// Dummy Functions
int pd_board_checks(void) {return EC_SUCCESS;}
void pd_set_input_current_limit(int port, uint32_t max_ma, uint32_t supply_voltage){}
int pd_custom_vdm(int port, int cnt, uint32_t *payload, uint32_t **rpayload){}
//	Request:0x230320C8
//	SinkCap:0x260190C8
void pd_execute_data_swap(int port, int data_role){}
int pd_check_power_swap(int port){}
int pd_check_data_swap(int port, int data_role){}
int pd_set_power_supply_ready(int port){return 0;}
void pd_check_dr_role(int port, int dr_role, int flags){}
void pd_check_pr_role(int port, int pr_role, int flags){}
int pd_snk_is_vbus_provided(int port) {return 1;}
static int max_mv, max_mv_index, max_mv_ma;
int pd_build_request(int port, uint32_t *rdo, uint32_t *ma, uint32_t *mv, enum pd_request_type req_type){
	*mv = max_mv;
	*ma = max_mv_ma;
	*rdo = RDO_FIXED(max_mv_index + 1, *ma, *ma, RDO_NO_SUSPEND | RDO_COMM_CAP);
	return EC_SUCCESS;
}

void pd_process_source_cap(int port, int cnt, uint32_t *src_caps)
{
	int i, mv, ma;

	max_mv = 0;
	max_mv_index = 0;
	for (i = 0; i < cnt; i++) {
		/* its an unsupported Augmented PDO (PD3.0) */
		if ((src_caps[i] & PDO_TYPE_MASK) == PDO_TYPE_AUGMENTED)
			continue;

		mv = ((src_caps[i] >> 10) & 0x3FF) * 50;

		/* Skip invalid voltage */
		if (!mv)
			continue;

		ma = (src_caps[i] & 0x3FF) * 10;
		if (mv > max_mv) {
			max_mv = mv;
			max_mv_index = i;
			max_mv_ma = ma;
		}
	}
}


// Dummy Variables
#define PDO_FIXED_FLAGS (PDO_FIXED_DUAL_ROLE | PDO_FIXED_DATA_SWAP | PDO_FIXED_COMM_CAP)
const uint32_t pd_snk_pdo[] = {
	PDO_FIXED(5000, 1500, PDO_FIXED_FLAGS),
};
const int pd_snk_pdo_cnt = 1;

/* TODO: determine the following board specific type-C power constants */
/*
 * delay to turn on the power supply max is ~16ms.
 * delay to turn off the power supply max is about ~180ms.
 */
#define PD_POWER_SUPPLY_TURN_ON_DELAY  30000  /* us */
#define PD_POWER_SUPPLY_TURN_OFF_DELAY 250000 /* us */

#define CPRINTF(format, args...)
#define CPRINTS(format, args...)

#ifdef CONFIG_USB_PD_DUAL_ROLE
#define DUAL_ROLE_IF_ELSE(port, sink_clause, src_clause) \
	(pd[port].power_role == PD_ROLE_SINK ? (sink_clause) : (src_clause))
#else
#define DUAL_ROLE_IF_ELSE(port, sink_clause, src_clause) (src_clause)
#endif
#define READY_RETURN_STATE(port) DUAL_ROLE_IF_ELSE(port, PD_STATE_SNK_READY, \
							 PD_STATE_SRC_READY)

/* Type C supply voltage (mV) */
#define TYPE_C_VOLTAGE	5000 /* mV */

/* PD counter definitions */
#define PD_MESSAGE_ID_COUNT 7
#define PD_HARD_RESET_COUNT 2
#define PD_CAPS_COUNT 50
#define PD_SNK_CAP_RETRIES 3

#ifdef CONFIG_USB_PD_USE_VDM
enum vdm_states {
	VDM_STATE_ERR_BUSY = -3,
	VDM_STATE_ERR_SEND = -2,
	VDM_STATE_ERR_TMOUT = -1,
	VDM_STATE_DONE = 0,
	/* Anything >0 represents an active state */
	VDM_STATE_READY = 1,
	VDM_STATE_BUSY = 2,
	VDM_STATE_WAIT_RSP_BUSY = 3,
};
#endif

#ifdef CONFIG_USB_PD_DUAL_ROLE
/* Port dual-role state */
enum pd_dual_role_states drp_state = CONFIG_USB_PD_INITIAL_DRP_STATE;

/* Enable variable for Try.SRC states */
static uint8_t pd_try_src_enable;
#endif

#ifdef CONFIG_USB_PD_REV30
/*
 * The spec. revision is used to index into this array.
 *  Rev 0 (PD 1.0) - return PD_CTRL_REJECT
 *  Rev 1 (PD 2.0) - return PD_CTRL_REJECT
 *  Rev 2 (PD 3.0) - return PD_CTRL_NOT_SUPPORTED
 */
static const uint8_t refuse[] = {
	PD_CTRL_REJECT, PD_CTRL_REJECT, PD_CTRL_NOT_SUPPORTED};
#define REFUSE(r) refuse[r]
#else
#define REFUSE(r) PD_CTRL_REJECT
#endif

#ifdef CONFIG_USB_PD_USE_VDM
#ifdef CONFIG_USB_PD_REV30
/*
 * The spec. revision is used to index into this array.
 *  Rev 0 (VDO 1.0) - return VDM_VER10
 *  Rev 1 (VDO 1.0) - return VDM_VER10
 *  Rev 2 (VDO 2.0) - return VDM_VER20
 */
static const uint8_t vdo_ver[] = {
	VDM_VER10, VDM_VER10, VDM_VER20};
#define VDO_VER(v) vdo_ver[v]
#else
#define VDO_VER(v) VDM_VER10
#endif
#endif // #ifdef CONFIG_USB_PD_USE_VDM

typedef int (*TX_DONE_CALLBACK)(int, int, void*);
enum pd_protocol_context {
	PDC_CA_SENT,
	PDC_VDM_SENT,
	PDC_BOARD_CHECKED,
	PDC_MESSAGE_HANDLED,
	PDC_STATE_MACHINE
};

static struct pd_protocol {
	/* current port power role (SOURCE or SINK) */
	uint8_t power_role;
	/* current port data role (DFP or UFP) */
	uint8_t data_role;
	/* 3-bit rolling message ID counter */
	uint8_t msg_id;
	/* Port polarity : 0 => CC1 is CC line, 1 => CC2 is CC line */
	uint8_t polarity;
	/* PD state for port */
	enum pd_states task_state;
	/* PD state when we run state handler the last time */
	enum pd_states last_state;
	/* The state to go to after timeout */
	enum pd_states timeout_state;
	/* port flags, see PD_FLAGS_* */
	uint32_t flags;
	/* Timeout for the current state. Set to 0 for no timeout. */
	uint64_t timeout;
	/* Time for source recovery after hard reset */
	uint64_t src_recover;
	/* Time for CC debounce end */
	uint64_t cc_debounce;
	/* The cc state */
	enum pd_cc_states cc_state;
	/* status of last transmit */
	uint8_t tx_status;

	/* last requested voltage PDO index */
	int requested_idx;
#ifdef CONFIG_USB_PD_FUNC_SNK
	/* Current limit / voltage based on the last request message */
	uint32_t curr_limit;
	uint32_t supply_voltage;
	/* Signal charging update that affects the port */
	int new_power_request;
	/* Store previously requested voltage request */
	int prev_request_mv;
#ifndef CONFIG_USB_PD_VBUS_DETECT_NONE
	int snk_hard_reset_vbus_off;
#endif
#endif
#ifdef CONFIG_USB_PD_DUAL_ROLE
	/* Time for Try.SRC states */
	uint64_t try_src_marker;
#endif

#ifdef CONFIG_USB_PD_USE_VDM
	/* PD state for Vendor Defined Messages */
	enum vdm_states vdm_state;
	/* Timeout for the current vdm state.  Set to 0 for no timeout. */
	uint64_t vdm_timeout;
	/* next Vendor Defined Message to send */
	uint32_t vdo_data[VDO_MAX_SIZE];
	uint8_t vdo_count;
	/* VDO to retry if UFP responder replied busy. */
	uint32_t vdo_retry;
#endif

#ifdef CONFIG_USB_PD_REV30
	/* PD Collision avoidance buffer */
	uint16_t ca_buffered;
	uint16_t ca_header;
	uint32_t ca_buffer[PDO_MAX_OBJECTS];
	enum tcpm_transmit_type ca_type;
	/* protocol revision */
	uint8_t rev;
#endif

	// Rikka's patch and internal states
	uint32_t pending_event;
	uint64_t next_protocol_run;		// After this time, we should run protocol while(1) again
	uint64_t tx_timeout;
	TX_DONE_CALLBACK tx_callback;
	void* tx_param;
	enum pd_states tx_saved_state;
	enum pd_protocol_context tx_prev_context;
#ifdef CONFIG_USB_PD_DUAL_ROLE
	uint64_t next_role_swap;
//#ifdef CONFIG_USB_PD_DUAL_ROLE_AUTO_TOGGLE
//	const int auto_toggle_supported = tcpm_auto_toggle_supported(port);
//#endif
//#if defined(CONFIG_CHARGE_MANAGER)
//	typec_current_t typec_curr = 0, typec_curr_change = 0;
//#endif /* CONFIG_CHARGE_MANAGER */
#endif /* CONFIG_USB_PD_DUAL_ROLE */
	int hard_reset_count;			// pd_task.hard_reset_count
	enum pd_cc_states new_cc_state;	// pd_task.new_cc_state
	int caps_count, hard_reset_sent;
	int snk_cap_count;
} pd[CONFIG_USB_PD_PORT_COUNT];

/**
 * Set a task event.
 *
 * If the task is higher priority than the current task, this will cause an
 * immediate context switch to the new task.
 *
 * Can be called both in interrupt context and task context.
 *
 * @param tskid		Task to set event for
 * @param event		Event bitmap to set (TASK_EVENT_*)
 * @param wait		If non-zero, after setting the event, de-schedule the
 *			calling task to wait for a response event.  Ignored in
 *			interrupt context.
 */
//#define task_set_event(port, event, wait) (pd[port].pending_event |= event)
void task_set_event(uint32_t port, uint32_t event, int wait)
{
	pd[port].pending_event |= event;
}

#ifdef CONFIG_COMMON_RUNTIME
uint8_t pd_comm_enabled[CONFIG_USB_PD_PORT_COUNT];
#define pd_comm_is_enabled(port) pd_comm_enabled[port]
#else
#define pd_comm_is_enabled(port) 1
#endif

static inline void set_state_timeout(int port,
				     uint64_t timeout,
				     enum pd_states timeout_state)
{
	pd[port].timeout = timeout;
	pd[port].timeout_state = timeout_state;
}

#ifdef CONFIG_USB_PD_REV30
int pd_get_rev(int port)
{
	return pd[port].rev;
}

int pd_get_vdo_ver(int port)
{
	return vdo_ver[pd[port].rev];
}
#endif

/* Return flag for pd state is connected */
int pd_is_connected(int port)
{
	if (pd[port].task_state == PD_STATE_DISABLED)
		return 0;

#ifdef CONFIG_USB_PD_DUAL_ROLE_AUTO_TOGGLE
	if (pd[port].task_state == PD_STATE_DRP_AUTO_TOGGLE)
		return 0;
#endif

	return DUAL_ROLE_IF_ELSE(port,
		/* sink */
		pd[port].task_state != PD_STATE_SNK_DISCONNECTED &&
		pd[port].task_state != PD_STATE_SNK_DISCONNECTED_DEBOUNCE,
		/* source */
		pd[port].task_state != PD_STATE_SRC_DISCONNECTED &&
		pd[port].task_state != PD_STATE_SRC_DISCONNECTED_DEBOUNCE);
}

/*
 * Return true if partner port is a DTS or TS capable of entering debug
 * mode (eg. is presenting Rp/Rp or Rd/Rd).
 */
int pd_ts_dts_plugged(int port)
{
	return pd[port].flags & PD_FLAGS_TS_DTS_PARTNER;
}

/* Return true if partner port is known to be PD capable. */
int pd_capable(int port)
{
	return pd[port].flags & PD_FLAGS_PREVIOUS_PD_CONN;
}

#ifdef CONFIG_USB_PD_FUNC_SNK
void pd_vbus_low(int port)
{
	pd[port].flags &= ~PD_FLAGS_VBUS_NEVER_LOW;
}

static inline int pd_is_vbus_present(int port)
{
#ifdef CONFIG_USB_PD_VBUS_DETECT_TCPC
	return tcpm_get_vbus_level(port);
#else
	return pd_snk_is_vbus_provided(port);
#endif
}
#endif

static void set_polarity(int port, int polarity)
{
	tcpm_set_polarity(port, polarity);
#ifdef CONFIG_USBC_PPC_POLARITY
	ppc_set_polarity(port, polarity);
#endif /* defined(CONFIG_USBC_PPC_POLARITY) */
}

#ifdef CONFIG_USBC_VCONN
static void set_vconn(int port, int enable)
{
	/*
	 * We always need to tell the TCPC to enable Vconn first, otherwise some
	 * TCPCs get confused and think the CC line is in over voltage mode and
	 * immediately disconnects. If there is a PPC, both devices will
	 * potentially source Vconn, but that should be okay since Vconn has
	 * "make before break" electrical requirements when swapping anyway.
	 */
	tcpm_set_vconn(port, enable);
#ifdef CONFIG_USBC_PPC_VCONN
	ppc_set_vconn(port, enable);
#endif
}
#endif /* defined(CONFIG_USBC_VCONN) */

static inline void set_state(int port, enum pd_states next_state)
{
	enum pd_states last_state = pd[port].task_state;

	set_state_timeout(port, 0, 0);
	pd[port].task_state = next_state;

	if (last_state == next_state)
		return;

#ifdef CONFIG_USB_PD_DUAL_ROLE
#ifdef CONFIG_USB_PD_DUAL_ROLE_AUTO_TOGGLE
	/* Clear flag to allow DRP auto toggle when possible */
	if (last_state != PD_STATE_DRP_AUTO_TOGGLE)
		pd[port].flags &= ~PD_FLAGS_TCPC_DRP_TOGGLE;
#endif

	/* Ignore dual-role toggling between sink and source */
	if ((last_state == PD_STATE_SNK_DISCONNECTED &&
	     next_state == PD_STATE_SRC_DISCONNECTED) ||
	    (last_state == PD_STATE_SRC_DISCONNECTED &&
	     next_state == PD_STATE_SNK_DISCONNECTED))
		return;
#endif

	if (
#ifdef CONFIG_USD_PD_FUNC_SRC
			(next_state == PD_STATE_SRC_DISCONNECTED)
#endif
#if defined(CONFIG_USB_PD_FUNC_SNK) && defined(CONFIG_USD_PD_FUNC_SRC)
			||
#endif
#ifdef CONFIG_USB_PD_FUNC_SNK
			(next_state == PD_STATE_SNK_DISCONNECTED)
#endif
		) {

#ifdef CONFIG_USB_PD_FUNC_SNK
		/* Clear the input current limit */
		pd_set_input_current_limit(port, 0, 0);
#ifdef CONFIG_CHARGE_MANAGER
		typec_set_input_current_limit(port, 0, 0);
		charge_manager_set_ceil(port,
					CEIL_REQUESTOR_PD,
					CHARGE_CEIL_NONE);
#endif
#endif // #ifdef CONFIG_USB_PD_FUNC_SNK

#ifdef CONFIG_USBC_VCONN
		set_vconn(port, 0);
#endif /* defined(CONFIG_USBC_VCONN) */

#ifdef CONFIG_USD_PD_FUNC_SRC
		/*
		 * If we are source, make sure VBUS is off and
		 * if PD REV3.0, restore RP.
		 */
		if (pd[port].power_role == PD_ROLE_SOURCE) {
			/*
			 * Rp is restored by pd_power_supply_reset if
			 * CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT is defined.
			 */
			pd_power_supply_reset(port);
#if !defined(CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT) && \
		defined(CONFIG_USB_PD_REV30)
			/* Restore Rp */
			tcpm_select_rp_value(port, CONFIG_USB_PD_PULLUP);
			tcpm_set_cc(port, TYPEC_CC_RP);
#endif
		}
#endif // #ifdef CONFIG_USD_PD_FUNC_SRC

#ifdef CONFIG_USB_PD_REV30
		/* Adjust rev to highest level*/
		pd[port].rev = PD_REV30;
#endif
		pd[port].flags &= ~PD_FLAGS_RESET_ON_DISCONNECT_MASK;
#if defined(CONFIG_USB_PD_DUAL_ROLE) && defined(CONFIG_CHARGE_MANAGER)
		charge_manager_update_dualrole(port, CAP_UNKNOWN);
#endif
#ifdef CONFIG_USB_PD_ALT_MODE_DFP
		pd_dfp_exit_mode(port, 0, 0);
#endif
#ifdef CONFIG_USBC_SS_MUX
		usb_mux_set(port, TYPEC_MUX_NONE, USB_SWITCH_DISCONNECT,
			    pd[port].polarity);
#endif
		/* Disable TCPC RX */
		tcpm_set_rx_enable(port, 0);
	}

	CPRINTF("C%d st%d %s\n", port, next_state, pd_state_names[next_state]);
}

#ifdef CONFIG_USB_PD_REV30
static void sink_can_xmit(int port, int rp)
{
	tcpm_select_rp_value(port, rp);
	tcpm_set_cc(port, TYPEC_CC_RP);
}

static inline void pd_ca_reset(int port)
{
	pd[port].ca_buffered = 0;
}
#endif

void pd_transmit_complete(int port, int status)
{
	if (status == TCPC_TX_COMPLETE_SUCCESS)
		/* increment message ID counter */
		pd[port].msg_id = (pd[port].msg_id + 1) & PD_MESSAGE_ID_COUNT;

	pd[port].tx_status = (status == TCPC_TX_COMPLETE_SUCCESS) ? 1 : -1;
	task_set_event(PD_PORT_TO_TASK_ID(port), PD_EVENT_TX, 0);
}

static void pd_transmit(int port, enum tcpm_transmit_type type,
		       uint16_t header, const uint32_t *data, TX_DONE_CALLBACK callback, void* param)
{
	pd[port].tx_callback = callback;
	pd[port].tx_param = param;
	pd[port].flags |= PD_FLAGS_TX_GOING_ON;
	pd[port].tx_timeout = timestamp_get() + PD_T_TCPC_TX_TIMEOUT;

	/* If comms are disabled, do not transmit, return error */
	if (!pd_comm_is_enabled(port)) {
		pd[port].tx_status = -1;
		return;
	}
#ifdef CONFIG_USB_PD_REV30
	/* Source-coordinated collision avoidance */
	/*
	 * In order to avoid message collisions due to asynchronous Messaging
	 * sent from the Sink, the Source sets Rp to SinkTxOk to indicate to
	 * the Sink that it is ok to initiate an AMS. When the Source wishes
	 * to initiate an AMS it sets Rp to SinkTxNG. When the Sink detects
	 * that Rp is set to SinkTxOk it May initiate an AMS. When the Sink
	 * detects that Rp is set to SinkTxNG it Shall Not initiate an AMS
	 * and Shall only send Messages that are part of an AMS the Source has
	 * initiated. Note that this restriction applies to SOP* AMS’s i.e.
	 * for both Port to Port and Port to Cable Plug communications.
	 *
	 * This starts after an Explicit Contract is in place
	 * PD R3 V1.1 Section 2.5.2.
	 *
	 * Note: a Sink can still send Hard Reset signaling at any time.
	 */
	if ((pd[port].rev == PD_REV30) &&
		(pd[port].flags & PD_FLAGS_EXPLICIT_CONTRACT)) {
		if (pd[port].power_role == PD_ROLE_SOURCE) {
			/*
			 * Inform Sink that it can't transmit. If a sink
			 * transmition is in progress and a collsion occurs,
			 * a reset is generated. This should be rare because
			 * all extended messages are chunked. This effectively
			 * defaults to PD REV 2.0 collision avoidance.
			 */
			sink_can_xmit(port, SINK_TX_NG);
		} else if (type != TCPC_TX_HARD_RESET) {
			int cc1;
			int cc2;

			tcpm_get_cc(port, &cc1, &cc2);
			if (cc1 == TYPEC_CC_VOLT_SNK_1_5 ||
				cc2 == TYPEC_CC_VOLT_SNK_1_5) {
				/* Sink can't transmit now. */
				/* Check if message is already buffered. */
				if (pd[port].ca_buffered) {
					pd[port].tx_status = -1;
					return;
				}

				/* Buffer message and send later. */
				pd[port].ca_type = type;
				pd[port].ca_header = header;
				memcpy(pd[port].ca_buffer,
					data, sizeof(uint32_t) *
					PD_HEADER_CNT(header));
				pd[port].ca_buffered = 1;
				pd[port].tx_status = 1;
				return;
			}
		}
	}
#endif
	tcpm_transmit(port, type, header, data);
}

#ifdef CONFIG_USB_PD_REV30
static int pd_txdone_sent_pending_ca(int port, int res, void* param) {
	if (pd[port].tx_status >= 0)
		/* Message was sent, so free up the buffer. */
		pd[port].ca_buffered = 0;

	return 0;
}

static int pd_ca_send_pending(int port)
{
	int cc1;
	int cc2;

	/* Check if a message has been buffered. */
	if (!pd[port].ca_buffered)
		return;

	tcpm_get_cc(port, &cc1, &cc2);
	if ((cc1 != TYPEC_CC_VOLT_SNK_1_5) &&
			(cc2 != TYPEC_CC_VOLT_SNK_1_5)) {

		pd_transmit(port, pd[port].ca_type,
						pd[port].ca_header,
						pd[port].ca_buffer,
						pd_txdone_sent_pending_ca, 0)
		return 1;
	}


	// TODO: BUG?? If the source does not allow us to send the message, should we just discard it?
	/* Message was sent, so free up the buffer. */
	pd[port].ca_buffered = 0;
	return 0;
}
#endif

static void pd_update_roles(int port)
{
	/* Notify TCPC of role update */
	tcpm_set_msg_header(port, pd[port].power_role, pd[port].data_role);
}

static void send_control(int port, int type, TX_DONE_CALLBACK callback, void* param)
{
	uint16_t header = PD_HEADER(type, pd[port].power_role,
				pd[port].data_role, pd[port].msg_id, 0,
				pd_get_rev(port), 0);

	pd_transmit(port, TCPC_TX_SOP, header, NULL, callback, param);
	// CPRINTF("C%d CTRL[%d]>%d\n", port, type, res);
}

#ifdef CONFIG_USD_PD_FUNC_SRC
static int pd_txdone_sent_src_cap(int port, int res, void* param) {
	CPRINTF("C%d srcCAP>%d\n", port, res);

	switch ((int)param) {	// mode
	case 0:	// 	case PD_CTRL_GET_SOURCE_CAP:
		if ((res >= 0) &&
		    (pd[port].task_state == PD_STATE_SRC_DISCOVERY))
			set_state(port, PD_STATE_SRC_NEGOCIATE);
		break;
	case 1: // 	case PD_STATE_SRC_DISCOVERY:
		/* packet was acked => PD capable device) */
		if (res >= 0) {
			set_state(port, PD_STATE_SRC_NEGOCIATE);
			pd[port].hard_reset_count = 0;
			pd[port].caps_count = 0;
			/* Port partner is PD capable */
			pd[port].flags |= PD_FLAGS_PREVIOUS_PD_CONN;
			return 10*MSEC;
		}
		/* failed, retry later */
		pd[port].caps_count++;
		return PD_T_SEND_SOURCE_CAP;
	case 2: // 	case PD_STATE_SRC_READY:
		if (res >= 0) {
			set_state(port, PD_STATE_SRC_NEGOCIATE);
			pd[port].flags &= ~PD_FLAGS_UPDATE_SRC_CAPS;
		}
		return PD_T_SOURCE_ACTIVITY;
	}

	return 0;
}

static void send_source_cap(int port, int mode)
{
#if defined(CONFIG_USB_PD_DYNAMIC_SRC_CAP) || \
		defined(CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT)
	const uint32_t *src_pdo;
	const int src_pdo_cnt = charge_manager_get_source_pdo(&src_pdo, port);
#else
	const uint32_t *src_pdo = pd_src_pdo;
	const int src_pdo_cnt = pd_src_pdo_cnt;
#endif
	uint16_t header;

	if (src_pdo_cnt == 0)
		/* No source capabilities defined, sink only */
		header = PD_HEADER(PD_CTRL_REJECT, pd[port].power_role,
			pd[port].data_role, pd[port].msg_id, 0,
			pd_get_rev(port), 0);
	else
		header = PD_HEADER(PD_DATA_SOURCE_CAP, pd[port].power_role,
			pd[port].data_role, pd[port].msg_id, src_pdo_cnt,
			pd_get_rev(port), 0);

	pd_transmit(port, TCPC_TX_SOP, header, src_pdo, pd_txdone_sent_src_cap, (void*)mode);
}
#endif

#ifdef CONFIG_USB_PD_REV30
static int send_battery_cap(int port, uint32_t *payload)
{
	int bit_len;
	uint16_t msg[6] = {0, 0, 0, 0, 0, 0};
	uint16_t header = PD_HEADER(PD_EXT_BATTERY_CAP,
				    pd[port].power_role,
				    pd[port].data_role,
				    pd[port].msg_id,
				    3, /* Number of Data Objects */
				    pd[port].rev,
				    1  /* This is an exteded message */
				   );

	/* Set extended header */
	msg[0] = PD_EXT_HEADER(0, /* Chunk Number */
			       0, /* Request Chunk */
			       9  /* Data Size in bytes */
			      );
	/* Set VID */
	msg[1] = USB_VID_GOOGLE;

	/* Set PID */
	msg[2] = CONFIG_USB_PID;

	if (battery_is_present()) {
		/*
		 * We only have one fixed battery,
		 * so make sure batt cap ref is 0.
		 */
		if (BATT_CAP_REF(payload[0]) != 0) {
			/* Invalid battery reference */
			msg[5] = 1;
		} else {
			uint32_t v;
			uint32_t c;

			/*
			 * The Battery Design Capacity field shall return the
			 * Battery’s design capacity in tenths of Wh. If the
			 * Battery is Hot Swappable and is not present, the
			 * Battery Design Capacity field shall be set to 0. If
			 * the Battery is unable to report its Design Capacity,
			 * it shall return 0xFFFF
			 */
			msg[3] = 0xffff;

			/*
			 * The Battery Last Full Charge Capacity field shall
			 * return the Battery’s last full charge capacity in
			 * tenths of Wh. If the Battery is Hot Swappable and
			 * is not present, the Battery Last Full Charge Capacity
			 * field shall be set to 0. If the Battery is unable to
			 * report its Design Capacity, the Battery Last Full
			 * Charge Capacity field shall be set to 0xFFFF.
			 */
			msg[4] = 0xffff;

			if (battery_design_voltage(&v) == 0) {
				if (battery_design_capacity(&c) == 0) {
					/*
					 * Wh = (c * v) / 1000000
					 * 10th of a Wh = Wh * 10
					 */
					msg[3] = DIV_ROUND_NEAREST((c * v),
								100000);
				}

				if (battery_full_charge_capacity(&c) == 0) {
					/*
					 * Wh = (c * v) / 1000000
					 * 10th of a Wh = Wh * 10
					 */
					msg[4] = DIV_ROUND_NEAREST((c * v),
								100000);
				}
			}
		}
	}

	bit_len = pd_transmit(port, TCPC_TX_SOP, header, (uint32_t *)msg);
	if (debug_level >= 2)
		CPRINTF("C%d batCap>%d\n", port, bit_len);
	return bit_len;
}

static int send_battery_status(int port,  uint32_t *payload)
{
	int bit_len;
	uint32_t msg = 0;
	uint16_t header = PD_HEADER(PD_DATA_BATTERY_STATUS,
				    pd[port].power_role,
				    pd[port].data_role,
				    pd[port].msg_id,
				    1, /* Number of Data Objects */
				    pd[port].rev,
				    0 /* This is NOT an extended message */
				  );

	if (battery_is_present()) {
		/*
		 * We only have one fixed battery,
		 * so make sure batt cap ref is 0.
		 */
		if (BATT_CAP_REF(payload[0]) != 0) {
			/* Invalid battery reference */
			msg |= BSDO_INVALID;
		} else {
			uint32_t v;
			uint32_t c;

			if (battery_design_voltage(&v) != 0 ||
					battery_remaining_capacity(&c) != 0) {
				msg |= BSDO_CAP(BSDO_CAP_UNKNOWN);
			} else {
				/*
				 * Wh = (c * v) / 1000000
				 * 10th of a Wh = Wh * 10
				 */
				msg |= BSDO_CAP(DIV_ROUND_NEAREST((c * v),
								100000));
			}

			/* Battery is present */
			msg |= BSDO_PRESENT;

			/*
			 * For drivers that are not smart battery compliant,
			 * battery_status() returns EC_ERROR_UNIMPLEMENTED and
			 * the battery is assumed to be idle.
			 */
			if (battery_status(&c) != 0) {
				msg |= BSDO_IDLE; /* assume idle */
			} else {
				if (c & STATUS_FULLY_CHARGED)
					/* Fully charged */
					msg |= BSDO_IDLE;
				else if (c & STATUS_DISCHARGING)
					/* Discharging */
					msg |= BSDO_DISCHARGING;
				/* else battery is charging.*/
			}
		}
	} else {
		msg = BSDO_CAP(BSDO_CAP_UNKNOWN);
	}

	bit_len = pd_transmit(port, TCPC_TX_SOP, header, &msg);
	// CPRINTF("C%d batStat>%d\n", port, bit_len);

	return bit_len;
}
#endif

#ifdef CONFIG_USB_PD_FUNC_SNK
static int pd_txdone_sent_snk_cap(int port, int res, void* param) {
	CPRINTF("C%d snkCAP>%d\n", port, outcome);
	return 0;
}

static void send_sink_cap(int port)
{
	uint16_t header = PD_HEADER(PD_DATA_SINK_CAP, pd[port].power_role,
			pd[port].data_role, pd[port].msg_id, pd_snk_pdo_cnt,
			pd_get_rev(port), 0);

	pd_transmit(port, TCPC_TX_SOP, header, pd_snk_pdo, pd_txdone_sent_snk_cap, 0);
}
#endif // #ifdef CONFIG_USB_PD_FUNC_SNK


#ifdef CONFIG_USB_PD_USE_VDM
static void queue_vdm(int port, uint32_t *header, const uint32_t *data,
			     int data_cnt)
{
	pd[port].vdo_count = data_cnt + 1;
	pd[port].vdo_data[0] = header[0];
	for (int i=0; i<data_cnt; i++) {
		pd[port].vdo_data[1+i] = data[i];
	}
	/* Set ready, pd task will actually send */
	pd[port].vdm_state = VDM_STATE_READY;
}

static void handle_vdm_request(int port, int cnt, uint32_t *payload)
{
	int rlen = 0;
	uint32_t *rdata;

	if (pd[port].vdm_state == VDM_STATE_BUSY) {
		/* If UFP responded busy retry after timeout */
		if (PD_VDO_CMDT(payload[0]) == CMDT_RSP_BUSY) {
			pd[port].vdm_timeout = timestamp_get() +
				PD_T_VDM_BUSY;
			pd[port].vdm_state = VDM_STATE_WAIT_RSP_BUSY;
			pd[port].vdo_retry = (payload[0] & ~VDO_CMDT_MASK) |
				CMDT_INIT;
			return;
		} else {
			pd[port].vdm_state = VDM_STATE_DONE;
		}
	}

	if (PD_VDO_SVDM(payload[0]))
		rlen = pd_svdm(port, cnt, payload, &rdata);
	else
		rlen = pd_custom_vdm(port, cnt, payload, &rdata);

	if (rlen > 0) {
		queue_vdm(port, rdata, &rdata[1], rlen - 1);
		return;
	}

	CPRINTF("C%d Unhandled VDM VID %04x CMD %04x\n", port, PD_VDO_VID(payload[0]), payload[0] & 0xFFFF);
}
#endif // #ifdef CONFIG_USB_PD_USE_VDM

void pd_execute_hard_reset(int port)
{
	if (pd[port].last_state == PD_STATE_HARD_RESET_SEND)
		CPRINTF("C%d HARD RST TX\n", port);
	else
		CPRINTF("C%d HARD RST RX\n", port);

	pd[port].msg_id = 0;
#ifdef CONFIG_USB_PD_ALT_MODE_DFP
	pd_dfp_exit_mode(port, 0, 0);
#endif

#ifdef CONFIG_USB_PD_REV30
	pd[port].rev = PD_REV30;
	pd_ca_reset(port);
#endif
	/*
	 * Fake set last state to hard reset to make sure that the next
	 * state to run knows that we just did a hard reset.
	 */
	pd[port].last_state = PD_STATE_HARD_RESET_EXECUTE;

#ifdef CONFIG_USB_PD_FUNC_SNK
	/*
	 * If we are swapping to a source and have changed to Rp, restore back
	 * to Rd and turn off vbus to match our power_role.
	 */
	if (pd[port].task_state == PD_STATE_SNK_SWAP_STANDBY ||
	    pd[port].task_state == PD_STATE_SNK_SWAP_COMPLETE) {
		tcpm_set_cc(port, TYPEC_CC_RD);
		pd_power_supply_reset(port);
	}

	if (pd[port].power_role == PD_ROLE_SINK) {
		/* Clear the input current limit */
		pd_set_input_current_limit(port, 0, 0);
#ifdef CONFIG_CHARGE_MANAGER
		charge_manager_set_ceil(port,
					CEIL_REQUESTOR_PD,
					CHARGE_CEIL_NONE);
#endif /* CONFIG_CHARGE_MANAGER */

		set_state(port, PD_STATE_SNK_HARD_RESET_RECOVER);
		return;
	}
#endif /* CONFIG_USB_PD_FUNC_SNK */

#ifdef CONFIG_USD_PD_FUNC_SRC
	/* We are a source, cut power */
	pd_power_supply_reset(port);
	pd[port].src_recover = timestamp_get() + PD_T_SRC_RECOVER;
	set_state(port, PD_STATE_SRC_HARD_RESET_RECOVER);
#endif // #ifdef CONFIG_USD_PD_FUNC_SRC
}

static void execute_soft_reset(int port)
{
	pd[port].msg_id = 0;
	set_state(port, DUAL_ROLE_IF_ELSE(port, PD_STATE_SNK_DISCOVERY,
						PD_STATE_SRC_DISCOVERY));
	CPRINTF("C%d Soft Rst\n", port);
}

void pd_soft_reset(void)
{
	int i;

	for (i = 0; i < CONFIG_USB_PD_PORT_COUNT; ++i)
		if (pd_is_connected(i)) {
			set_state(i, PD_STATE_SOFT_RESET);
			task_set_event(PD_PORT_TO_TASK_ID(i), TASK_EVENT_WAKE, 0);
		}
}

#ifdef CONFIG_USB_PD_FUNC_SNK
static int pd_txdone_request_new_pwr(int port, int res, void* param) {
	if (res < 0 ) {
		if (res != EC_SUCCESS && ((int)param) == 0)
			set_state(port, PD_STATE_SOFT_RESET);
		return 0;
	}
	set_state(port, PD_STATE_SNK_REQUESTED);

	return 0;
}

/*
 * Request desired charge voltage from source.
 * Returns EC_SUCCESS on success or non-zero on failure.
 */
static int pd_send_request_msg(int port, int always_send_request)
{
	uint32_t rdo, curr_limit, supply_voltage;
	int res;

#ifdef CONFIG_CHARGE_MANAGER
	int charging = (charge_manager_get_active_charge_port() == port);
#else
	const int charging = 1;
#endif

#ifdef CONFIG_USB_PD_CHECK_MAX_REQUEST_ALLOWED
	int max_request_allowed = pd_is_max_request_allowed();
#else
	const int max_request_allowed = 1;
#endif

	/* Clear new power request */
	pd[port].new_power_request = 0;

	/* Build and send request RDO */
	/*
	 * If this port is not actively charging or we are not allowed to
	 * request the max voltage, then select vSafe5V
	 */
	res = pd_build_request(port, &rdo, &curr_limit, &supply_voltage,
			       charging && max_request_allowed ?
					PD_REQUEST_MAX : PD_REQUEST_VSAFE5V);

	if (res != EC_SUCCESS)
		/*
		 * If fail to choose voltage, do nothing, let source re-send
		 * source cap
		 */
		return -1;

	if (!always_send_request) {
		/* Don't re-request the same voltage */
		if (pd[port].prev_request_mv == supply_voltage)
			return EC_SUCCESS;
#ifdef CONFIG_CHARGE_MANAGER
		/* Limit current to PD_MIN_MA during transition */
		else
			charge_manager_force_ceil(port, PD_MIN_MA);
#endif
	}

	CPRINTF("C%d Req [%d] %dmV %dmA", port, RDO_POS(rdo),
		supply_voltage, curr_limit);
	if (rdo & RDO_CAP_MISMATCH)
		CPRINTF(" Mismatch");
	CPRINTF("\n");

	pd[port].curr_limit = curr_limit;
	pd[port].supply_voltage = supply_voltage;
	pd[port].prev_request_mv = supply_voltage;

	uint16_t header = PD_HEADER(PD_DATA_REQUEST, pd[port].power_role,
			pd[port].data_role, pd[port].msg_id, 1,
			pd_get_rev(port), 0);

	pd_transmit(port, TCPC_TX_SOP, header, &rdo, pd_txdone_request_new_pwr, (void*)always_send_request);
	return 1;
}
#endif

static void pd_update_pdo_flags(int port, uint32_t pdo)
{
#ifdef CONFIG_CHARGE_MANAGER
#ifdef CONFIG_USB_PD_ALT_MODE_DFP
	int charge_whitelisted =
		(pd[port].power_role == PD_ROLE_SINK &&
		 pd_charge_from_device(pd_get_identity_vid(port),
				       pd_get_identity_pid(port)));
#else
	const int charge_whitelisted = 0;
#endif
#endif

	/* can only parse PDO flags if type is fixed */
	if ((pdo & PDO_TYPE_MASK) != PDO_TYPE_FIXED)
		return;

#ifdef CONFIG_USB_PD_FUNC_SNK
	if (pdo & PDO_FIXED_DUAL_ROLE)
		pd[port].flags |= PD_FLAGS_PARTNER_DR_POWER;
	else
		pd[port].flags &= ~PD_FLAGS_PARTNER_DR_POWER;

	if (pdo & PDO_FIXED_EXTERNAL)
		pd[port].flags |= PD_FLAGS_PARTNER_EXTPOWER;
	else
		pd[port].flags &= ~PD_FLAGS_PARTNER_EXTPOWER;

	if (pdo & PDO_FIXED_COMM_CAP)
		pd[port].flags |= PD_FLAGS_PARTNER_USB_COMM;
	else
		pd[port].flags &= ~PD_FLAGS_PARTNER_USB_COMM;
#endif

	if (pdo & PDO_FIXED_DATA_SWAP)
		pd[port].flags |= PD_FLAGS_PARTNER_DR_DATA;
	else
		pd[port].flags &= ~PD_FLAGS_PARTNER_DR_DATA;

#ifdef CONFIG_CHARGE_MANAGER
	/*
	 * Treat device as a dedicated charger (meaning we should charge
	 * from it) if it does not support power swap, or if it is externally
	 * powered, or if we are a sink and the device identity matches a
	 * charging white-list.
	 */
	if (!(pd[port].flags & PD_FLAGS_PARTNER_DR_POWER) ||
	    (pd[port].flags & PD_FLAGS_PARTNER_EXTPOWER) ||
	    charge_whitelisted)
		charge_manager_update_dualrole(port, CAP_DEDICATED);
	else
		charge_manager_update_dualrole(port, CAP_DUALROLE);
#endif
}

#ifdef CONFIG_USD_PD_FUNC_SRC
static int pd_txdone_data_req_handled(int port, int res, void* param) {
	if (res < 0)
		/*
		 * if we fail to send accept, do
		 * nothing and let sink timeout and
		 * send hard reset
		 */
		return 0;

	/* explicit contract is now in place */
	pd[port].flags |= PD_FLAGS_EXPLICIT_CONTRACT;
#ifdef CONFIG_USB_PD_REV30
	/*
	 * Start Source-coordinated collision
	 * avoidance
	 */
	if (pd[port].rev == PD_REV30 &&
		pd[port].power_role == PD_ROLE_SOURCE)
		sink_can_xmit(port, SINK_TX_OK);
#endif
	pd[port].requested_idx = RDO_POS((uint32_t)param);
	set_state(port, PD_STATE_SRC_ACCEPTED);
	return 0;
}

static int pd_txdone_data_req_rejected(int port, int res, void* param) {
	/* keep last contract in place (whether implicit or explicit) */
	set_state(port, PD_STATE_SRC_READY);
	return 0;
}
#endif // #ifdef CONFIG_USD_PD_FUNC_SRC

static int handle_data_request(int port, uint16_t head,
		uint32_t *payload)
{
	int type = PD_HEADER_TYPE(head);
	int cnt = PD_HEADER_CNT(head);

	switch (type) {

#ifdef CONFIG_USB_PD_FUNC_SNK
	case PD_DATA_SOURCE_CAP:
		if ((pd[port].task_state == PD_STATE_SNK_DISCOVERY)
			|| (pd[port].task_state == PD_STATE_SNK_TRANSITION)
#ifdef CONFIG_USB_PD_VBUS_DETECT_NONE
			|| (pd[port].task_state ==
			    PD_STATE_SNK_HARD_RESET_RECOVER)
#endif
			|| (pd[port].task_state == PD_STATE_SNK_READY)) {
#ifdef CONFIG_USB_PD_REV30
			/*
			 * Only adjust sink rev if source rev is higher.
			 */
			if (PD_HEADER_REV(head) < pd[port].rev)
				pd[port].rev = PD_HEADER_REV(head);
#endif
			/* Port partner is now known to be PD capable */
			pd[port].flags |= PD_FLAGS_PREVIOUS_PD_CONN;

			/* src cap 0 should be fixed PDO */
			pd_update_pdo_flags(port, payload[0]);

			pd_process_source_cap(port, cnt, payload);

			/* Source will resend source cap on failure */
			pd_send_request_msg(port, 1);
		}
		return 1;
#endif /* CONFIG_USB_PD_FUNC_SNK */

#ifdef CONFIG_USD_PD_FUNC_SRC
	case PD_DATA_REQUEST:
		if ((pd[port].power_role == PD_ROLE_SOURCE) && (cnt == 1)) {
#ifdef CONFIG_USB_PD_REV30
			/*
			 * Adjust the rev level to what the sink supports. If
			 * they're equal, no harm done.
			 */
			pd[port].rev = PD_HEADER_REV(head);
#endif
			if (!pd_check_requested_voltage(payload[0], port)) {
				send_control(port, PD_CTRL_ACCEPT, pd_txdone_data_req_handled, (void*)payload[0]);
				return 1;
			}
		}
		/* the message was incorrect or cannot be satisfied */
		send_control(port, PD_CTRL_REJECT, pd_txdone_data_req_rejected, NULL);
		return 1;

	case PD_DATA_SINK_CAP:
		pd[port].flags |= PD_FLAGS_SNK_CAP_RECVD;
		/* snk cap 0 should be fixed PDO */
		pd_update_pdo_flags(port, payload[0]);
		if (pd[port].task_state == PD_STATE_SRC_GET_SINK_CAP)
			set_state(port, PD_STATE_SRC_READY);
		break;
#endif // #ifdef CONFIG_USD_PD_FUNC_SRC

#ifdef CONFIG_USB_PD_REV30
	case PD_DATA_BATTERY_STATUS:
		break;
#endif
#ifdef CONFIG_USB_PD_USE_VDM
	case PD_DATA_VENDOR_DEF:
		handle_vdm_request(port, cnt, payload);
		break;
#endif
	default:
		CPRINTF("C%d Unhandled data message type %d\n", port, type);
	}
	return 0;
}

#ifdef CONFIG_USB_PD_DUAL_ROLE
void pd_request_power_swap(int port)
{
	if (pd[port].task_state == PD_STATE_SRC_READY)
		set_state(port, PD_STATE_SRC_SWAP_INIT);
	else if (pd[port].task_state == PD_STATE_SNK_READY)
		set_state(port, PD_STATE_SNK_SWAP_INIT);
	task_wake(PD_PORT_TO_TASK_ID(port));
}

#ifdef CONFIG_USBC_VCONN_SWAP
static void pd_request_vconn_swap(int port)
{
	if (pd[port].task_state == PD_STATE_SRC_READY ||
	    pd[port].task_state == PD_STATE_SNK_READY)
		set_state(port, PD_STATE_VCONN_SWAP_SEND);
	task_wake(PD_PORT_TO_TASK_ID(port));
}

void pd_try_vconn_src(int port)
{
	/*
	 * If we don't currently provide vconn, and we can supply it, send
	 * a vconn swap request.
	 */
	if (!(pd[port].flags & PD_FLAGS_VCONN_ON)) {
		if (pd_check_vconn_swap(port))
			pd_request_vconn_swap(port);
	}
}
#endif
#endif /* CONFIG_USB_PD_DUAL_ROLE */

#ifdef CONFIG_USB_PD_DR_SWAP
void pd_request_data_swap(int port)
{
	if (DUAL_ROLE_IF_ELSE(port,
				pd[port].task_state == PD_STATE_SNK_READY,
				pd[port].task_state == PD_STATE_SRC_READY))
		set_state(port, PD_STATE_DR_SWAP);
	task_wake(PD_PORT_TO_TASK_ID(port));
}
#endif // CONFIG_USB_PD_DR_SWAP

static void pd_set_data_role(int port, int role)
{
	pd[port].data_role = role;
	pd_execute_data_swap(port, role);

#ifdef CONFIG_USBC_SS_MUX
#ifdef CONFIG_USBC_SS_MUX_DFP_ONLY
	/*
	 * Need to connect SS mux for if new data role is DFP.
	 * If new data role is UFP, then disconnect the SS mux.
	 */
	if (role == PD_ROLE_DFP)
		usb_mux_set(port, TYPEC_MUX_USB, USB_SWITCH_CONNECT,
			    pd[port].polarity);
	else
		usb_mux_set(port, TYPEC_MUX_NONE, USB_SWITCH_DISCONNECT,
			    pd[port].polarity);
#else
	usb_mux_set(port, TYPEC_MUX_USB, USB_SWITCH_CONNECT,
		    pd[port].polarity);
#endif
#endif
	pd_update_roles(port);
}

static void pd_dr_swap(int port)
{
	pd_set_data_role(port, !pd[port].data_role);
	pd[port].flags |= PD_FLAGS_CHECK_IDENTITY;
}

#ifdef CONFIG_USB_PD_DUAL_ROLE
static int pd_txdone_accept_pr_swap(int port, int res, void* param) {
	/*
	 * Clear flag for checking power role to avoid
	 * immediately requesting another swap.
	 */
	pd[port].flags &= ~PD_FLAGS_CHECK_PR_ROLE;
	set_state(port,
		  DUAL_ROLE_IF_ELSE(port,
			PD_STATE_SNK_SWAP_SNK_DISABLE,
			PD_STATE_SRC_SWAP_SNK_DISABLE));
	return 0;
}
#endif // #ifdef CONFIG_USB_PD_DUAL_ROLE

#ifdef CONFIG_USB_PD_DR_SWAP
static int pd_txdone_accept_dr_swap(int port, int res, void* param) {
	if (res >= 0)
		pd_dr_swap(port);
	return 0;
}
#endif // #ifdef CONFIG_USB_PD_DR_SWAP

#ifdef CONFIG_USBC_VCONN_SWAP
static int pd_txdone_accept_vconn_swap(int port, int res, void* param) {
	if (res > 0)
		set_state(port, PD_STATE_VCONN_SWAP_INIT);
}
#endif /* CONFIG_USBC_VCONN_SWAP */

static int handle_ctrl_request(int port, uint16_t head,
		uint32_t *payload)
{
	int type = PD_HEADER_TYPE(head);

	switch (type) {
	case PD_CTRL_GOOD_CRC:
		/* should not get it */
		break;
	case PD_CTRL_PING:
		/* Nothing else to do */
		break;
	case PD_CTRL_NOT_SUPPORTED:
		/* PD3.0 only, we should not get it*/
		break;

#ifdef CONFIG_USD_PD_FUNC_SRC
	case PD_CTRL_GET_SOURCE_CAP:
		send_source_cap(port, 0);
		return 1;
#endif

	case PD_CTRL_GET_SINK_CAP:
#ifdef CONFIG_USB_PD_FUNC_SNK
		send_sink_cap(port);
#else
		send_control(port, REFUSE(pd[port].rev), NULL, NULL);
#endif
		return 1;


	case PD_CTRL_GOTO_MIN:
#ifdef CONFIG_USB_PD_FUNC_SNK

#ifdef CONFIG_USB_PD_GIVE_BACK
		if (pd[port].task_state == PD_STATE_SNK_READY) {
			/*
			 * Reduce power consumption now!
			 *
			 * The source will restore power to this sink
			 * by sending a new source cap message at a
			 * later time.
			 */
			pd_snk_give_back(port, &pd[port].curr_limit,
				&pd[port].supply_voltage);
			set_state(port, PD_STATE_SNK_TRANSITION);
		}
		break;
#endif  // #ifdef CONFIG_USB_PD_GIVE_BACK

#else
		send_control(port, REFUSE(pd[port].rev), NULL, NULL);
		return 1;
#endif	// #ifdef CONFIG_USB_PD_FUNC_SNK

	case PD_CTRL_PS_RDY:
		if (pd[port].task_state == PD_STATE_SNK_SWAP_SRC_DISABLE) {
			set_state(port, PD_STATE_SNK_SWAP_STANDBY);
#ifdef CONFIG_USB_PD_DUAL_ROLE
		} else if (pd[port].task_state == PD_STATE_SRC_SWAP_STANDBY) {
			/* reset message ID and swap roles */
			pd[port].msg_id = 0;
			pd[port].power_role = PD_ROLE_SINK;
			pd_update_roles(port);
			set_state(port, PD_STATE_SNK_DISCOVERY);
#endif // #ifdef CONFIG_USB_PD_DUAL_ROLE
#ifdef CONFIG_USBC_VCONN_SWAP
		} else if (pd[port].task_state == PD_STATE_VCONN_SWAP_INIT) {
			/*
			 * If VCONN is on, then this PS_RDY tells us it's
			 * ok to turn VCONN off
			 */
			if (pd[port].flags & PD_FLAGS_VCONN_ON)
				set_state(port, PD_STATE_VCONN_SWAP_READY);
#endif
#ifdef CONFIG_USB_PD_FUNC_SNK
		} else if (pd[port].task_state == PD_STATE_SNK_DISCOVERY) {
			/* Don't know what power source is ready. Reset. */
			set_state(port, PD_STATE_HARD_RESET_SEND);
		} else if (pd[port].task_state == PD_STATE_SNK_SWAP_STANDBY) {
			/* Do nothing, assume this is a redundant PD_RDY */
		} else if (pd[port].power_role == PD_ROLE_SINK) {
			set_state(port, PD_STATE_SNK_READY);
			pd_set_input_current_limit(port, pd[port].curr_limit,
						   pd[port].supply_voltage);
#ifdef CONFIG_CHARGE_MANAGER
			/* Set ceiling based on what's negotiated */
			charge_manager_set_ceil(port,
						CEIL_REQUESTOR_PD,
						pd[port].curr_limit);
#endif
#endif // #ifdef CONFIG_USB_PD_FUNC_SNK
		}
		break;

	case PD_CTRL_REJECT:
	case PD_CTRL_WAIT:
		if (pd[port].task_state == PD_STATE_DR_SWAP) {
			if (type == PD_CTRL_WAIT) /* try again ... */
				pd[port].flags |= PD_FLAGS_CHECK_DR_ROLE;
			set_state(port, READY_RETURN_STATE(port));
		}
#ifdef CONFIG_USBC_VCONN_SWAP
		else if (pd[port].task_state == PD_STATE_VCONN_SWAP_SEND)
			set_state(port, READY_RETURN_STATE(port));
#endif
#ifdef CONFIG_USB_PD_DUAL_ROLE
		else if (pd[port].task_state == PD_STATE_SRC_SWAP_INIT)
			set_state(port, PD_STATE_SRC_READY);
		else if (pd[port].task_state == PD_STATE_SNK_SWAP_INIT)
			set_state(port, PD_STATE_SNK_READY);
		else if (pd[port].task_state == PD_STATE_SNK_REQUESTED) {
			/*
			 * Explicit Contract in place
			 *
			 *  On reception of a WAIT message, transition to
			 *  PD_STATE_SNK_READY after PD_T_SINK_REQUEST ms to
			 *  send another reqest.
			 *
			 *  On reception of a REJECT messag, transition to
			 *  PD_STATE_SNK_READY but don't resend the request.
			 *
			 * NO Explicit Contract in place
			 *
			 *  On reception of a WAIT or REJECT message,
			 *  transition to PD_STATE_SNK_DISCOVERY
			 */
			if (pd[port].flags & PD_FLAGS_EXPLICIT_CONTRACT) {
				/* We have an explicit contract */
				if (type == PD_CTRL_WAIT) {
					/*
					 * Trigger a new power request when
					 * we enter PD_STATE_SNK_READY
					 */
					pd[port].new_power_request = 1;

					/*
					 * After the request is triggered,
					 * make sure the request is sent.
					 */
					pd[port].prev_request_mv = 0;

					/*
					 * Transition to PD_STATE_SNK_READY
					 * after PD_T_SINK_REQUEST ms.
					 */
					set_state_timeout(port, timestamp_get() +
							PD_T_SINK_REQUEST,
							PD_STATE_SNK_READY);
				} else {
					/* The request was rejected */
					set_state(port, PD_STATE_SNK_READY);
				}
			} else {
				/* No explicit contract */
				set_state(port, PD_STATE_SNK_DISCOVERY);
			}
		}
#endif
		break;
	case PD_CTRL_ACCEPT:
		if (pd[port].task_state == PD_STATE_SOFT_RESET) {
			/*
			 * For the case that we sent soft reset in SNK_DISCOVERY
			 * on startup due to VBUS never low, clear the flag.
			 */
			pd[port].flags &= ~PD_FLAGS_VBUS_NEVER_LOW;
			execute_soft_reset(port);
		} else if (pd[port].task_state == PD_STATE_DR_SWAP) {
			/* switch data role */
			pd_dr_swap(port);
			set_state(port, READY_RETURN_STATE(port));
#ifdef CONFIG_USB_PD_DUAL_ROLE
#ifdef CONFIG_USBC_VCONN_SWAP
		} else if (pd[port].task_state == PD_STATE_VCONN_SWAP_SEND) {
			/* switch vconn */
			set_state(port, PD_STATE_VCONN_SWAP_INIT);
#endif
		} else if (pd[port].task_state == PD_STATE_SRC_SWAP_INIT) {
			/* explicit contract goes away for power swap */
			pd[port].flags &= ~PD_FLAGS_EXPLICIT_CONTRACT;
			set_state(port, PD_STATE_SRC_SWAP_SNK_DISABLE);
		} else if (pd[port].task_state == PD_STATE_SNK_SWAP_INIT) {
			/* explicit contract goes away for power swap */
			pd[port].flags &= ~PD_FLAGS_EXPLICIT_CONTRACT;
			set_state(port, PD_STATE_SNK_SWAP_SNK_DISABLE);
#endif
		} else if (pd[port].task_state == PD_STATE_SNK_REQUESTED) {
			/* explicit contract is now in place */
			pd[port].flags |= PD_FLAGS_EXPLICIT_CONTRACT;
			set_state(port, PD_STATE_SNK_TRANSITION);
		}
		break;
	case PD_CTRL_SOFT_RESET:
		execute_soft_reset(port);
		/* We are done, acknowledge with an Accept packet */
		send_control(port, PD_CTRL_ACCEPT, NULL, NULL);
		return 1;
	case PD_CTRL_PR_SWAP:
#ifdef CONFIG_USB_PD_DUAL_ROLE
		if (pd_check_power_swap(port))
			send_control(port, PD_CTRL_ACCEPT, pd_txdone_accept_pr_swap, NULL);
		else
			send_control(port, REFUSE(pd[port].rev), NULL, NULL);
#else
		send_control(port, REFUSE(pd[port].rev), NULL, NULL);
#endif
		return 1;

	case PD_CTRL_DR_SWAP:
#ifdef CONFIG_USB_PD_DR_SWAP
		if (pd_check_data_swap(port, pd[port].data_role)) {
			/*
			 * Accept switch and perform data swap. Clear
			 * flag for checking data role to avoid
			 * immediately requesting another swap.
			 */
			pd[port].flags &= ~PD_FLAGS_CHECK_DR_ROLE;
			send_control(port, PD_CTRL_ACCEPT, pd_txdone_accept_dr_swap, NULL);
		} else {
			send_control(port, REFUSE(pd[port].rev), NULL, NULL);
		}
#else
		send_control(port, REFUSE(pd[port].rev), NULL, NULL);
#endif // CONFIG_USB_PD_DR_SWAP
		return 1;

	case PD_CTRL_VCONN_SWAP:
#ifdef CONFIG_USBC_VCONN_SWAP
		if (pd[port].task_state == PD_STATE_SRC_READY ||
		    pd[port].task_state == PD_STATE_SNK_READY) {
			if (pd_check_vconn_swap(port))
				send_control(port, PD_CTRL_ACCEPT, pd_txdone_accept_vconn_swap, NULL);
			else
				send_control(port, REFUSE(pd[port].rev), NULL, NULL);
		}
#else
		send_control(port, REFUSE(pd[port].rev), NULL, NULL);
#endif
		return 1;

	default:
#ifdef CONFIG_USB_PD_REV30
		send_control(port, PD_CTRL_NOT_SUPPORTED);
		return 1;
#endif
		CPRINTF("C%d Unhandled ctrl message type %d\n", port, type);
	}

	return 0;
}

#ifdef CONFIG_USB_PD_REV30
// TODO: need to be fixed
static int handle_ext_request(int port, uint16_t head, uint32_t *payload)
{
	int type = PD_HEADER_TYPE(head);

	switch (type) {
	case PD_EXT_GET_BATTERY_CAP:
		send_battery_cap(port, payload);
		break;
	case PD_EXT_GET_BATTERY_STATUS:
		send_battery_status(port, payload);
		break;
	case PD_EXT_BATTERY_CAP:
		break;
	default:
		send_control(port, PD_CTRL_NOT_SUPPORTED);
	}
}
#endif

// Return 1 if we send something and need to wait for its outcome
static int handle_request(int port, uint16_t head,
		uint32_t *payload)
{
	int cnt = PD_HEADER_CNT(head);
//	int p;

	/* dump received packet content (only dump ping at debug level 3) */
//	if ((debug_level == 2 && PD_HEADER_TYPE(head) != PD_CTRL_PING) ||
//	    debug_level >= 3) {
//		CPRINTF("C%d RECV %04x/%d ", port, head, cnt);
//		for (p = 0; p < cnt; p++)
//			CPRINTF("[%d]%08x ", p, payload[p]);
//		CPRINTF("\n");
//	}

	/*
	 * If we are in disconnected state, we shouldn't get a request. Do
	 * a hard reset if we get one.
	 */
	if (!pd_is_connected(port))
		set_state(port, PD_STATE_HARD_RESET_SEND);

#ifdef CONFIG_USB_PD_REV30
	/* Check if this is an extended chunked data message. */
	if (pd[port].rev == PD_REV30 && PD_HEADER_EXT(head))
		return handle_ext_request(port, head, payload);

#endif
	if (cnt)
		return handle_data_request(port, head, payload);
	else
		return handle_ctrl_request(port, head, payload);
}

#ifdef CONFIG_USB_PD_USE_VDM
void pd_send_vdm(int port, uint32_t vid, int cmd, const uint32_t *data,
		 int count)
{
	if (count > VDO_MAX_SIZE - 1) {
		CPRINTF("C%d VDM over max size\n", port);
		return;
	}

	/* set VDM header with VID & CMD */
	pd[port].vdo_data[0] = VDO(vid, ((vid & USB_SID_PD) == USB_SID_PD) ?
				   1 : (PD_VDO_CMD(cmd) <= CMD_ATTENTION), cmd);
#ifdef CONFIG_USB_PD_REV30
	pd[port].vdo_data[0] |= VDO_SVDM_VERS(vdo_ver[pd[port].rev]);
#endif
	queue_vdm(port, pd[port].vdo_data, data, count);

	task_set_event(PD_PORT_TO_TASK_ID(port), TASK_EVENT_WAKE, 0);
}

static inline int pdo_busy(int port)
{
	/*
	 * Note, main PDO state machine (pd_task) uses READY state exclusively
	 * to denote port partners have successfully negociated a contract.  All
	 * other protocol actions force state transitions.
	 */
	int rv = (pd[port].task_state != PD_STATE_SRC_READY);
#ifdef CONFIG_USB_PD_DUAL_ROLE
	rv &= (pd[port].task_state != PD_STATE_SNK_READY);
#endif
	return rv;
}

static uint64_t vdm_get_ready_timeout(uint32_t vdm_hdr)
{
	uint64_t timeout;
	int cmd = PD_VDO_CMD(vdm_hdr);

	/* its not a structured VDM command */
	if (!PD_VDO_SVDM(vdm_hdr))
		return 500*MSEC;

	switch (PD_VDO_CMDT(vdm_hdr)) {
	case CMDT_INIT:
		if ((cmd == CMD_ENTER_MODE) || (cmd == CMD_EXIT_MODE))
			timeout = PD_T_VDM_WAIT_MODE_E;
		else
			timeout = PD_T_VDM_SNDR_RSP;
		break;
	default:
		if ((cmd == CMD_ENTER_MODE) || (cmd == CMD_EXIT_MODE))
			timeout = PD_T_VDM_E_MODE;
		else
			timeout = PD_T_VDM_RCVR_RSP;
		break;
	}
	return timeout;
}

static int pd_txdone_sent_vdm(int port, int res, void* param) {
	if (res < 0) {
		pd[port].vdm_state = VDM_STATE_ERR_SEND;
	} else {
		pd[port].vdm_state = VDM_STATE_BUSY;
		pd[port].vdm_timeout = timestamp_get() +
			vdm_get_ready_timeout(pd[port].vdo_data[0]);
	}
	return 0;
}

static int pd_vdm_send_state_machine(int port)
{
	uint16_t header;

	switch (pd[port].vdm_state) {
	case VDM_STATE_READY:
		/* Only transmit VDM if connected. */
		if (!pd_is_connected(port)) {
			pd[port].vdm_state = VDM_STATE_ERR_BUSY;
			return 0;
		}

		/*
		 * if there's traffic or we're not in PDO ready state don't send
		 * a VDM.
		 */
		if (pdo_busy(port))
			return 0;

		/* Prepare and send VDM */
		header = PD_HEADER(PD_DATA_VENDOR_DEF, pd[port].power_role,
				   pd[port].data_role, pd[port].msg_id,
				   (int)pd[port].vdo_count,
				   pd_get_rev(port), 0);
		pd_transmit(port, TCPC_TX_SOP, header,
				  pd[port].vdo_data, pd_txdone_sent_vdm, NULL);
		return 1;
	case VDM_STATE_WAIT_RSP_BUSY:
		/* wait and then initiate request again */
		if (timestamp_get() > pd[port].vdm_timeout) {
			pd[port].vdo_data[0] = pd[port].vdo_retry;
			pd[port].vdo_count = 1;
			pd[port].vdm_state = VDM_STATE_READY;
		}
		return 0;
	case VDM_STATE_BUSY:
		/* Wait for VDM response or timeout */
		if (pd[port].vdm_timeout &&
		    (timestamp_get() > pd[port].vdm_timeout)) {
			pd[port].vdm_state = VDM_STATE_ERR_TMOUT;
		}
		return 0;
	default:
		return 0;
	}
}
#endif // #ifdef CONFIG_USB_PD_USE_VDM

#ifdef CONFIG_USB_PD_DUAL_ROLE
enum pd_dual_role_states pd_get_dual_role(void)
{
	return drp_state;
}

#ifdef CONFIG_USB_PD_TRY_SRC
static void pd_update_try_source(void)
{
	int i;

#ifndef CONFIG_CHARGER
	int batt_soc = board_get_battery_soc();
#else
	int batt_soc = charge_get_percent();
#endif

	/*
	 * Enable try source when dual-role toggling AND battery is present
	 * and at some minimum percentage.
	 */
	pd_try_src_enable = drp_state == PD_DRP_TOGGLE_ON &&
			    batt_soc >= CONFIG_USB_PD_TRY_SRC_MIN_BATT_SOC;
#if defined(CONFIG_BATTERY_PRESENT_CUSTOM) || \
	defined(CONFIG_BATTERY_PRESENT_GPIO)
	/*
	 * When battery is cutoff in ship mode it may not be reliable to
	 * check if battery is present with its state of charge.
	 * Also check if battery is initialized and ready to provide power.
	 */
	pd_try_src_enable &= (battery_is_present() == BP_YES);
#endif

	/*
	 * Clear this flag to cover case where a TrySrc
	 * mode went from enabled to disabled and trying_source
	 * was active at that time.
	 */
	for (i = 0; i < CONFIG_USB_PD_PORT_COUNT; i++)
		pd[i].flags &= ~PD_FLAGS_TRY_SRC;

}
// DECLARE_HOOK(HOOK_BATTERY_SOC_CHANGE, pd_update_try_source, HOOK_PRIO_DEFAULT);
#endif

void pd_set_dual_role(enum pd_dual_role_states state)
{
	int i;
	drp_state = state;

#ifdef CONFIG_USB_PD_TRY_SRC
	pd_update_try_source();
#endif

	/* Inform PD tasks of dual role change. */
	for (i = 0; i < CONFIG_USB_PD_PORT_COUNT; i++)
		task_set_event(PD_PORT_TO_TASK_ID(i),
			       PD_EVENT_UPDATE_DUAL_ROLE, 0);
}

void pd_update_dual_role_config(int port)
{
	/*
	 * Change to sink if port is currently a source AND (new DRP
	 * state is force sink OR new DRP state is either toggle off
	 * or debug accessory toggle only and we are in the source
	 * disconnected state).
	 */
	if (pd[port].power_role == PD_ROLE_SOURCE &&
	    ((drp_state == PD_DRP_FORCE_SINK && !pd_ts_dts_plugged(port)) ||
	     (drp_state == PD_DRP_TOGGLE_OFF
	      && pd[port].task_state == PD_STATE_SRC_DISCONNECTED))) {
		pd[port].power_role = PD_ROLE_SINK;
		set_state(port, PD_STATE_SNK_DISCONNECTED);
		tcpm_set_cc(port, TYPEC_CC_RD);
		/* Make sure we're not sourcing VBUS. */
		pd_power_supply_reset(port);
	}

	/*
	 * Change to source if port is currently a sink and the
	 * new DRP state is force source.
	 */
	if (pd[port].power_role == PD_ROLE_SINK &&
	    drp_state == PD_DRP_FORCE_SOURCE) {
		pd[port].power_role = PD_ROLE_SOURCE;
		set_state(port, PD_STATE_SRC_DISCONNECTED);
		tcpm_set_cc(port, TYPEC_CC_RP);
	}

#if defined(CONFIG_USB_PD_DUAL_ROLE_AUTO_TOGGLE) && \
	defined(CONFIG_USB_PD_TCPC_LOW_POWER)
	/* When switching drp mode, make sure tcpc is out of standby mode */
	tcpm_set_drp_toggle(port, 0);
#endif
}

int pd_get_role(int port)
{
	return pd[port].power_role;
}

static int pd_is_power_swapping(int port)
{
	/* return true if in the act of swapping power roles */
	return  pd[port].task_state == PD_STATE_SNK_SWAP_SNK_DISABLE ||
		pd[port].task_state == PD_STATE_SNK_SWAP_SRC_DISABLE ||
		pd[port].task_state == PD_STATE_SNK_SWAP_STANDBY ||
		pd[port].task_state == PD_STATE_SNK_SWAP_COMPLETE ||
		pd[port].task_state == PD_STATE_SRC_SWAP_SNK_DISABLE ||
		pd[port].task_state == PD_STATE_SRC_SWAP_SRC_DISABLE ||
		pd[port].task_state == PD_STATE_SRC_SWAP_STANDBY;
}

/*
 * Provide Rp to ensure the partner port is in a known state (eg. not
 * PD negotiated, not sourcing 20V).
 */
static void pd_partner_port_reset(int port)
{
	uint64_t timeout;

	/*
	 * Check our battery-backed previous port state. If PD comms were
	 * active, and we didn't just lose power, make sure we
	 * don't boot into RO with a pre-existing power contract.
	 */

	/*
	 * Clear the active contract bit before we apply Rp in case we
	 * intentionally brown out because we cut off our only power supply.
	 */
	pd_set_saved_active(port, 0);

	/* Provide Rp for 200 msec. or until we no longer have VBUS. */
	tcpm_set_cc(port, TYPEC_CC_RP);
	timeout = timestamp_get() + 200 * MSEC;

	while (timestamp_get() < timeout && pd_is_vbus_present(port))
		msleep(10);
}
#endif /* CONFIG_USB_PD_DUAL_ROLE */

int pd_get_polarity(int port)
{
	return pd[port].polarity;
}

int pd_get_partner_data_swap_capable(int port)
{
	/* return data swap capable status of port partner */
	return pd[port].flags & PD_FLAGS_PARTNER_DR_DATA;
}

#ifdef CONFIG_COMMON_RUNTIME
void pd_comm_enable(int port, int enable)
{
	/* We don't check port >= CONFIG_USB_PD_PORT_COUNT deliberately */
	pd_comm_enabled[port] = enable;

	/* If type-C connection, then update the TCPC RX enable */
	if (pd_is_connected(port))
		tcpm_set_rx_enable(port, enable);

#ifdef CONFIG_USB_PD_DUAL_ROLE
	/*
	 * If communications are enabled, start hard reset timer for
	 * any port in PD_SNK_DISCOVERY.
	 */
	if (enable && pd[port].task_state == PD_STATE_SNK_DISCOVERY)
		set_state_timeout(port,
				  timestamp_get() + PD_T_SINK_WAIT_CAP,
				  PD_STATE_HARD_RESET_SEND);
#endif
}
#endif

void pd_ping_enable(int port, int enable)
{
	if (enable)
		pd[port].flags |= PD_FLAGS_PING_ENABLED;
	else
		pd[port].flags &= ~PD_FLAGS_PING_ENABLED;
}

/**
 * Returns whether the sink has detected a Rp resistor on the other side.
 */
static inline int cc_is_rp(int cc)
{
	return (cc == TYPEC_CC_VOLT_SNK_DEF) || (cc == TYPEC_CC_VOLT_SNK_1_5) ||
	       (cc == TYPEC_CC_VOLT_SNK_3_0);
}

/*
 * CC values for regular sources and Debug sources (aka DTS)
 *
 * Source type  Mode of Operation   CC1    CC2
 * ---------------------------------------------
 * Regular      Default USB Power   RpUSB  Open
 * Regular      USB-C @ 1.5 A       Rp1A5  Open
 * Regular      USB-C @ 3 A         Rp3A0  Open
 * DTS          Default USB Power   Rp3A0  Rp1A5
 * DTS          USB-C @ 1.5 A       Rp1A5  RpUSB
 * DTS          USB-C @ 3 A         Rp3A0  RpUSB
*/

/**
 * Returns the polarity of a Sink.
 */
static inline int get_snk_polarity(int cc1, int cc2)
{
	/* the following assumes:
	 * TYPEC_CC_VOLT_SNK_3_0 > TYPEC_CC_VOLT_SNK_1_5
	 * TYPEC_CC_VOLT_SNK_1_5 > TYPEC_CC_VOLT_SNK_DEF
	 * TYPEC_CC_VOLT_SNK_DEF > TYPEC_CC_VOLT_OPEN
	 */
	return (cc2 > cc1);
}

#if defined(CONFIG_CHARGE_MANAGER)
/**
 * Returns type C current limit (mA) based upon cc_voltage (mV).
 */
static typec_current_t get_typec_current_limit(int polarity, int cc1, int cc2)
{
	typec_current_t charge;
	int cc = polarity ? cc2 : cc1;
	int cc_alt = polarity ? cc1 : cc2;

	if (cc == TYPEC_CC_VOLT_SNK_3_0 && cc_alt != TYPEC_CC_VOLT_SNK_1_5)
		charge = 3000;
	else if (cc == TYPEC_CC_VOLT_SNK_1_5)
		charge = 1500;
	else
		charge = 0;

	if (cc_is_rp(cc_alt))
		charge |= TYPEC_CURRENT_DTS_MASK;

	return charge;
}

/**
 * Signal power request to indicate a charger update that affects the port.
 */
void pd_set_new_power_request(int port)
{
	pd[port].new_power_request = 1;
	task_wake(PD_PORT_TO_TASK_ID(port));
}
#endif /* CONFIG_CHARGE_MANAGER */

#if defined(CONFIG_USBC_BACKWARDS_COMPATIBLE_DFP) && defined(CONFIG_USBC_SS_MUX)
/*
 * Backwards compatible DFP does not support USB SS because it applies VBUS
 * before debouncing CC and setting USB SS muxes, but SS detection will fail
 * before we are done debouncing CC.
 */
#error "Backwards compatible DFP does not support USB"
#endif

#ifdef CONFIG_COMMON_RUNTIME

/* Initialize globals based on system state. */
static void pd_init_tasks(void)
{
	static int initialized;
	int enable = 1;
	int i;

	/* Initialize globals once, for all PD tasks.  */
	if (initialized)
		return;

#if defined(CONFIG_USB_PD_COMM_DISABLED)
	enable = 0;
#elif defined(CONFIG_USB_PD_COMM_LOCKED)
	/* Disable PD communication at init if we're in RO and locked. */
	if (!system_is_in_rw() && system_is_locked())
		enable = 0;
#ifdef CONFIG_VBOOT_EFS
	if (vboot_need_pd_comm())
		enable = 1;
#endif
#endif
	for (i = 0; i < CONFIG_USB_PD_PORT_COUNT; i++)
		pd_comm_enabled[i] = enable;
	CPRINTS("PD comm %sabled", enable ? "en" : "dis");

	initialized = 1;
}
#endif /* CONFIG_COMMON_RUNTIME */

static int pd_txdone_sent_hrst(int port, int res, void* param) {
	if (res < 0) {
		return 10*MSEC;
	}

	/* successfully sent hard reset */
	pd[port].hard_reset_sent = 1;

	/*
	 * If we are source, delay before cutting power
	 * to allow sink time to get hard reset.
	 */
	if (pd[port].power_role == PD_ROLE_SOURCE) {
		set_state_timeout(port, timestamp_get() + PD_T_PS_HARD_RESET,
		  PD_STATE_HARD_RESET_EXECUTE);
	} else {
		set_state(port, PD_STATE_HARD_RESET_EXECUTE);
		return 10*MSEC;
	}

	return 500*MSEC;
}

static int pd_txdone_sent_srst(int port, int res, void* param) {
	/* if soft reset failed, try hard reset. */
	if (res < 0) {
		set_state(port,
			  PD_STATE_HARD_RESET_SEND);
		return 5*MSEC;
	}

	set_state_timeout(
		port,
		timestamp_get() + PD_T_SENDER_RESPONSE,
		PD_STATE_HARD_RESET_SEND);

	return 0;
}

#ifdef CONFIG_USD_PD_FUNC_SRC
static int pd_txdone_sent_get_sink_cap(int port, int res, void* param) {
	set_state(port, PD_STATE_SRC_GET_SINK_CAP);
	return PD_T_SOURCE_ACTIVITY;
}

static int pd_txdone_sent_ps_rdy(int port, int res, void* param) {
	if (res >= 0) {
		/* it'a time to ping regularly the sink */
		set_state(port, PD_STATE_SRC_READY);
		return 10*MSEC;
	} else {
		/* The sink did not ack, cut the power... */
		set_state(port, PD_STATE_SRC_DISCONNECTED);
	}
	return 0;
}

#ifdef CONFIG_USD_PD_SRC_SEND_PING
static int pd_txdone_ping_sent(int port, int res, void* param) {
	if (res < 0) {
		/* Ping dropped. Try soft reset. */
		set_state(port, PD_STATE_SOFT_RESET);
		return 10 * MSEC;
	}
	return 0;
}
#endif // #ifdef CONFIG_USD_PD_SRC_SEND_PING
#endif // #ifdef CONFIG_USD_PD_FUNC_SRC

void pd_protocol_init() {
	int port = 0;
	//pd[port].next_role_swap = PD_T_DRP_SNK;
#if defined(CONFIG_USB_PD_FUNC_SNK) && !defined(CONFIG_USB_PD_VBUS_DETECT_NONE)
	pd[port].snk_hard_reset_vbus_off = 0;
#endif
	pd[port].caps_count = 0;
	pd[port].hard_reset_sent = 0;
	pd[port].snk_cap_count = 0;

	int res;

#if defined(CONFIG_CHARGE_MANAGER)
	typec_current_t typec_curr = 0, typec_curr_change = 0;
#endif /* CONFIG_CHARGE_MANAGER */
	enum pd_states this_state;


#ifdef CONFIG_COMMON_RUNTIME
	pd_init_tasks();
#endif

	/* Ensure the power supply is in the default state */
	pd_power_supply_reset(port);

	/* Initialize TCPM driver and wait for TCPC to be ready */
	res = tcpm_init(port);

#ifdef CONFIG_USB_PD_DUAL_ROLE
	pd_partner_port_reset(port);
#endif

	CPRINTS("TCPC p%d init %s", port, res ? "failed" : "ready");
	this_state = res ? PD_STATE_SUSPENDED : PD_DEFAULT_STATE(port);
//#ifndef CONFIG_USB_PD_TCPC
//	if (!res) {
//		struct ec_response_pd_chip_info *info;
//		tcpm_get_chip_info(port, 0, &info);
//		CPRINTS("TCPC p%d VID:0x%x PID:0x%x DID:0x%x FWV:0x%lx",
//			port, info->vendor_id, info->product_id,
//			info->device_id, info->fw_version_number);
//	}
//#endif

#ifdef CONFIG_USB_PD_REV30
	/* Set Revision to highest */
	pd[port].rev = PD_REV30;
	pd_ca_reset(port);
#endif


#ifdef CONFIG_USB_PD_DUAL_ROLE
	/*
	 * If VBUS is high, then initialize flag for VBUS has always been
	 * present. This flag is used to maintain a PD connection after a
	 * reset by sending a soft reset.
	 */
	pd[port].flags = pd_is_vbus_present(port) ? PD_FLAGS_VBUS_NEVER_LOW : 0;
#endif

	/* Disable TCPC RX until connection is established */
	tcpm_set_rx_enable(port, 0);

#ifdef CONFIG_USBC_SS_MUX
	/* Initialize USB mux to its default state */
	usb_mux_init(port);
#endif

	/* Initialize PD protocol state variables for each port. */
	pd[port].power_role = PD_ROLE_DEFAULT(port);
#ifdef CONFIG_USB_PD_USE_VDM
	pd[port].vdm_state = VDM_STATE_DONE;
#endif // #ifdef CONFIG_USB_PD_USE_VDM
	set_state(port, this_state);
#ifdef CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT
	ASSERT(PD_ROLE_DEFAULT(port) == PD_ROLE_SINK);
	tcpm_select_rp_value(port, CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT);
#else
	tcpm_select_rp_value(port, CONFIG_USB_PD_PULLUP);
#endif
	tcpm_set_cc(port, PD_ROLE_DEFAULT(port) == PD_ROLE_SOURCE ?
		    TYPEC_CC_RP : TYPEC_CC_RD);

#ifdef CONFIG_USBC_PPC
	/*
	 * Wait to initialize the PPC after setting the correct Rd values in
	 * the TCPC otherwise the TCPC might not be pulling the CC lines down
	 * when the PPC connects the CC lines from the USB connector to the
	 * TCPC cause the source to drop Vbus causing a brown out.
	 */
	ppc_init(port);
#endif

#ifdef CONFIG_USB_PD_ALT_MODE_DFP
	/* Initialize PD Policy engine */
	pd_dfp_pe_init(port);
#endif

#ifdef CONFIG_CHARGE_MANAGER
	/* Initialize PD and type-C supplier current limits to 0 */
	pd_set_input_current_limit(port, 0, 0);
	typec_set_input_current_limit(port, 0, 0);
	charge_manager_update_dualrole(port, CAP_UNKNOWN);
#endif
}

void pd_protocol_run()
{
	const int port = 0;
	uint64_t now = timestamp_get();
	uint64_t timeout = 0;

	if (pd[port].flags & PD_FLAGS_TX_GOING_ON) {
		if (now > pd[port].tx_timeout) {
			// Timeout
			pd[port].tx_status = -1;
			pd[port].flags &=~ PD_FLAGS_TX_GOING_ON;
		} else if (pd[port].pending_event & PD_EVENT_TX) {
			pd[port].pending_event &=~ PD_EVENT_TX;
			pd[port].flags &=~ PD_FLAGS_TX_GOING_ON;
		} else {
			return;	// Tx state should block the protocol task
		}

#ifdef CONFIG_USB_PD_REV30
	/*
	 * If the source just completed a transmit, tell
	 * the sink it can transmit if it wants to.
	 */
	if ((pd[port].rev == PD_REV30) &&
			(pd[port].power_role == PD_ROLE_SOURCE) &&
			(pd[port].flags & PD_FLAGS_EXPLICIT_CONTRACT)) {
		sink_can_xmit(port, SINK_TX_OK);
	}
#endif

		if (pd[port].tx_callback)
			timeout = pd[port].tx_callback(port, (int)(pd[port].tx_status), pd[port].tx_param);

		// Set the default timeout
		if (timeout == 0)
			timeout = 10*MSEC;

		switch(pd[port].tx_prev_context) {
#ifdef CONFIG_USB_PD_REV30
		case PDC_CA_SENT:
			goto EOS_CA_SENT;
#endif

#ifdef CONFIG_USB_PD_USE_VDM
		case PDC_VDM_SENT:
			goto EOS_VDM_SENT;
#endif

		case PDC_BOARD_CHECKED:
			goto EOS_BOARD_CHECKED;
		case PDC_MESSAGE_HANDLED:
			goto EOS_MESSAGE_HANDLED;
		case PDC_STATE_MACHINE:
			pd[port].last_state = pd[port].tx_saved_state;
			pd[port].tx_saved_state = PD_STATE_COUNT;	// Mark tx_saved_state as invalid
			goto EOS_STATE_MACHINE;
		default:
			return;	// This should not happen
		}
	}

#ifdef CONFIG_USB_PD_REV30
	/* send any pending messages */
	if (pd_ca_send_pending(port)) {
		pd[port].tx_prev_context = PDC_CA_SENT;
		return;	// We need to send the cached messaged
	}
EOS_CA_SENT:
#endif

#ifdef CONFIG_USB_PD_USE_VDM
	/* process VDM messages last */
	if (pd_vdm_send_state_machine(port)) {
		pd[port].tx_prev_context = PDC_VDM_SENT;
		return;
	}
EOS_VDM_SENT:
#endif

	/* Verify board specific health status : current, voltages... */
	if (pd_board_checks() != EC_SUCCESS) {
		/* cut the power */
		pd_execute_hard_reset(port);
		/* notify the other side of the issue */
		pd_transmit(port, TCPC_TX_HARD_RESET, 0, NULL, 0, 0);
		pd[port].tx_prev_context = PDC_BOARD_CHECKED;
		return;
	}


	/* wait for next event/packet or timeout expiration */
	int evt;
EOS_BOARD_CHECKED:
	evt = (now > pd[port].next_protocol_run) ? TASK_EVENT_TIMER : 0;
	evt |= pd[port].pending_event;
	if (evt == 0)
		return;	// Nothing happens
	pd[port].pending_event = 0;	// Clear all pending events
	pd[port].tx_saved_state = PD_STATE_COUNT;	// Mark tx_saved_state as invalid

	int cc1, cc2;
	int head;
	uint32_t payload[7];
#ifdef CONFIG_USB_PD_DUAL_ROLE_AUTO_TOGGLE
	const int auto_toggle_supported = tcpm_auto_toggle_supported(port);
#endif

#ifdef CONFIG_USB_PD_DUAL_ROLE
		if (evt & PD_EVENT_UPDATE_DUAL_ROLE)
			pd_update_dual_role_config(port);
#endif

#ifdef CONFIG_USB_PD_HANDLE_TCPC_RESET
		/* if TCPC has reset, then need to initialize it again */
		if (evt & PD_EVENT_TCPC_RESET) {
			CPRINTS("TCPC p%d reset!", port);
			if (tcpm_init(port) != EC_SUCCESS)
				CPRINTS("TCPC p%d init failed", port);
#ifdef CONFIG_USB_PD_DUAL_ROLE_AUTO_TOGGLE
		}

		if ((evt & PD_EVENT_TCPC_RESET) &&
		    (pd[port].task_state != PD_STATE_DRP_AUTO_TOGGLE)) {
#endif
			/* Ensure CC termination is default */
			tcpm_set_cc(port, PD_ROLE_DEFAULT(port) ==
				    PD_ROLE_SOURCE ? TYPEC_CC_RP : TYPEC_CC_RD);

			/*
			 * If we have a stable contract in the default role,
			 * then simply update TCPC with some missing info
			 * so that we can continue without resetting PD comms.
			 * Otherwise, go to the default disconnected state
			 * and force renegotiation.
			 */
			if (
#ifdef CONFIG_USB_PD_USE_VDM
					(pd[port].vdm_state == VDM_STATE_DONE) &&
#endif // #ifdef CONFIG_USB_PD_USE_VDM

				(
#ifdef CONFIG_USB_PD_FUNC_SNK
			     (PD_ROLE_DEFAULT(port) == PD_ROLE_SINK &&
			     pd[port].task_state == PD_STATE_SNK_READY)
#endif
#if defined(CONFIG_USB_PD_FUNC_SNK) && defined(CONFIG_USD_PD_FUNC_SRC)
				 ||
#endif
#ifdef CONFIG_USD_PD_FUNC_SRC
			     (PD_ROLE_DEFAULT(port) == PD_ROLE_SOURCE &&
			     pd[port].task_state == PD_STATE_SRC_READY)
#endif
				 )
			) {
				set_polarity(port, pd[port].polarity);
				tcpm_set_msg_header(port, pd[port].power_role,
						    pd[port].data_role);
				tcpm_set_rx_enable(port, 1);
			} else {
				/* Ensure state variables are at default */
				pd[port].power_role = PD_ROLE_DEFAULT(port);
#ifdef CONFIG_USB_PD_USE_VDM
				pd[port].vdm_state = VDM_STATE_DONE;
#endif // #ifdef CONFIG_USB_PD_USE_VDM
				set_state(port, PD_DEFAULT_STATE(port));
			}
		}
#endif // #ifdef CONFIG_USB_PD_HANDLE_TCPC_RESET


		pd[port].tx_prev_context = PDC_MESSAGE_HANDLED;
		/* process any potential incoming message */
		if (evt & PD_EVENT_RX) {
			if (!tcpm_get_message(port, payload, &head)) {
				if (handle_request(port, head, payload))
					return;	// We send something and need to wait for the outcome
			}
		}

EOS_MESSAGE_HANDLED:
		pd[port].tx_prev_context = PDC_STATE_MACHINE;

		/* if nothing to do, verify the state of the world in 500ms */
		enum pd_states this_state = pd[port].task_state;
		pd[port].tx_saved_state = this_state;	// Save the current state
		timeout = 500*MSEC;
		switch (this_state) {
		case PD_STATE_DISABLED:
			/* Nothing to do */
			break;
#ifdef CONFIG_USD_PD_FUNC_SRC
		case PD_STATE_SRC_DISCONNECTED:
			timeout = 10*MSEC;
			tcpm_get_cc(port, &cc1, &cc2);
#ifdef CONFIG_USB_PD_DUAL_ROLE_AUTO_TOGGLE
			/*
			 * Attempt TCPC auto DRP toggle if it is
			 * not already auto toggling and not try.src
			 */
			if (auto_toggle_supported &&
			    !(pd[port].flags & PD_FLAGS_TCPC_DRP_TOGGLE) &&
			    !(pd[port].flags & PD_FLAGS_TRY_SRC) &&
			    (cc1 == TYPEC_CC_VOLT_OPEN &&
			     cc2 == TYPEC_CC_VOLT_OPEN)) {
				set_state(port, PD_STATE_DRP_AUTO_TOGGLE);
				timeout = 2*MSEC;
				break;
			}
#endif

			/* Vnc monitoring */
			if ((cc1 == TYPEC_CC_VOLT_RD ||
			     cc2 == TYPEC_CC_VOLT_RD) ||
			    (cc1 == TYPEC_CC_VOLT_RA &&
			     cc2 == TYPEC_CC_VOLT_RA)) {
#ifdef CONFIG_USBC_BACKWARDS_COMPATIBLE_DFP
				/* Enable VBUS */
				if (pd_set_power_supply_ready(port))
					break;
#endif
				pd[port].cc_state = PD_CC_NONE;
				set_state(port,
					PD_STATE_SRC_DISCONNECTED_DEBOUNCE);
			}
#if defined(CONFIG_USB_PD_DUAL_ROLE)
			/*
			 * Try.SRC state is embedded here. Wait for SNK
			 * detect, or if timer expires, transition to
			 * SNK_DISCONNETED.
			 *
			 * If Try.SRC state is not active, then this block
			 * handles the normal DRP toggle from SRC->SNK
			 */
			else if ((pd[port].flags & PD_FLAGS_TRY_SRC &&
				 timestamp_get() >= pd[port].try_src_marker) ||
				 (!(pd[port].flags & PD_FLAGS_TRY_SRC) &&
				  drp_state != PD_DRP_FORCE_SOURCE &&
				  drp_state != PD_DRP_FREEZE &&
				 timestamp_get() >= pd[port].next_role_swap)) {
				pd[port].power_role = PD_ROLE_SINK;
				set_state(port, PD_STATE_SNK_DISCONNECTED);
				tcpm_set_cc(port, TYPEC_CC_RD);
				pd[port].next_role_swap = timestamp_get() + PD_T_DRP_SNK;
				pd[port].try_src_marker = timestamp_get()
					+ PD_T_TRY_WAIT;

				/* Swap states quickly */
				timeout = 2*MSEC;
			}
#endif
			break;
		case PD_STATE_SRC_DISCONNECTED_DEBOUNCE:
			timeout = 20*MSEC;
			tcpm_get_cc(port, &cc1, &cc2);

			if (cc1 == TYPEC_CC_VOLT_RD &&
			    cc2 == TYPEC_CC_VOLT_RD) {
				/* Debug accessory */
				pd[port].new_cc_state = PD_CC_DEBUG_ACC;
			} else if (cc1 == TYPEC_CC_VOLT_RD ||
				   cc2 == TYPEC_CC_VOLT_RD) {
				/* UFP attached */
				pd[port].new_cc_state = PD_CC_UFP_ATTACHED;
			} else if (cc1 == TYPEC_CC_VOLT_RA &&
				   cc2 == TYPEC_CC_VOLT_RA) {
				/* Audio accessory */
				pd[port].new_cc_state = PD_CC_AUDIO_ACC;
			} else {
				/* No UFP */
				set_state(port, PD_STATE_SRC_DISCONNECTED);
				timeout = 5*MSEC;
				break;
			}
			/* If in Try.SRC state, then don't need to debounce */
			if (!(pd[port].flags & PD_FLAGS_TRY_SRC)) {
				/* Debounce the cc state */
				if (pd[port].new_cc_state != pd[port].cc_state) {
					pd[port].cc_debounce = timestamp_get() +
						PD_T_CC_DEBOUNCE;
					pd[port].cc_state = pd[port].new_cc_state;
					break;
				} else if (timestamp_get() <
					   pd[port].cc_debounce) {
					break;
				}
			}

			/* Debounce complete */
			/* UFP is attached */
			if (pd[port].new_cc_state == PD_CC_UFP_ATTACHED ||
					pd[port].new_cc_state == PD_CC_DEBUG_ACC) {
				pd[port].polarity = (cc1 != TYPEC_CC_VOLT_RD);
				set_polarity(port, pd[port].polarity);

				/* initial data role for source is DFP */
				pd_set_data_role(port, PD_ROLE_DFP);

				if (pd[port].new_cc_state == PD_CC_DEBUG_ACC)
					pd[port].flags |= PD_FLAGS_TS_DTS_PARTNER;

#ifdef CONFIG_USBC_VCONN
				/*
				 * Start sourcing Vconn before Vbus to ensure
				 * we are within USB Type-C Spec 1.3 tVconnON
				 */
				set_vconn(port, 1);
				pd[port].flags |= PD_FLAGS_VCONN_ON;
#endif

#ifndef CONFIG_USBC_BACKWARDS_COMPATIBLE_DFP
				/* Enable VBUS */
				if (pd_set_power_supply_ready(port)) {
#ifdef CONFIG_USBC_VCONN
					/* Stop sourcing Vconn if Vbus failed */
					set_vconn(port, 0);
					pd[port].flags &= ~PD_FLAGS_VCONN_ON;
#endif /* CONFIG_USBC_VCONN */
#ifdef CONFIG_USBC_SS_MUX
					usb_mux_set(port, TYPEC_MUX_NONE,
						    USB_SWITCH_DISCONNECT,
						    pd[port].polarity);
#endif /* CONFIG_USBC_SS_MUX */
					break;
				}
#endif /* CONFIG_USBC_BACKWARDS_COMPATIBLE_DFP */
				/* If PD comm is enabled, enable TCPC RX */
				if (pd_comm_is_enabled(port))
					tcpm_set_rx_enable(port, 1);

				pd[port].flags |= PD_FLAGS_CHECK_PR_ROLE |
						  PD_FLAGS_CHECK_DR_ROLE;
				pd[port].hard_reset_count = 0;
				timeout = 5*MSEC;
				set_state(port, PD_STATE_SRC_STARTUP);
			}
			/*
			 * AUDIO_ACC will remain in this state indefinitely
			 * until disconnect.
			 */
			break;
		case PD_STATE_SRC_HARD_RESET_RECOVER:
			/* Do not continue until hard reset recovery time */
			if (timestamp_get() < pd[port].src_recover) {
				timeout = 50*MSEC;
				break;
			}

			/* Enable VBUS */
			timeout = 10*MSEC;
			if (pd_set_power_supply_ready(port)) {
				set_state(port, PD_STATE_SRC_DISCONNECTED);
				break;
			}
#ifdef CONFIG_USB_PD_TCPM_TCPCI
			/*
			 * After transmitting hard reset, TCPM writes
			 * to RECEIVE_DETECT register to enable
			 * PD message passing.
			 */
			if (pd_comm_is_enabled(port))
				tcpm_set_rx_enable(port, 1);
#endif /* CONFIG_USB_PD_TCPM_TCPCI */

			set_state(port, PD_STATE_SRC_STARTUP);
			break;
		case PD_STATE_SRC_STARTUP:
			/* Wait for power source to enable */
			if (pd[port].last_state != pd[port].task_state) {
				pd[port].flags |= PD_FLAGS_CHECK_IDENTITY;
				/* reset various counters */
				pd[port].caps_count = 0;
				pd[port].msg_id = 0;
				pd[port].snk_cap_count = 0;
				set_state_timeout(
					port,
#ifdef CONFIG_USBC_BACKWARDS_COMPATIBLE_DFP
					/*
					 * delay for power supply to start up.
					 * subtract out debounce time if coming
					 * from debounce state since vbus is
					 * on during debounce.
					 */
					timestamp_get() +
					PD_POWER_SUPPLY_TURN_ON_DELAY -
					  (pd[port].last_state ==
					   PD_STATE_SRC_DISCONNECTED_DEBOUNCE
						? PD_T_CC_DEBOUNCE : 0),
#else
					timestamp_get() +
					PD_POWER_SUPPLY_TURN_ON_DELAY,
#endif
					PD_STATE_SRC_DISCOVERY);
			}
			break;
		case PD_STATE_SRC_DISCOVERY:
			if (pd[port].last_state != pd[port].task_state) {
				/*
				 * If we have had PD connection with this port
				 * partner, then start NoResponseTimer.
				 */
				if (pd_capable(port))
					set_state_timeout(port,
						timestamp_get() +
						PD_T_NO_RESPONSE,
						pd[port].hard_reset_count <
						  PD_HARD_RESET_COUNT ?
						    PD_STATE_HARD_RESET_SEND :
						    PD_STATE_SRC_DISCONNECTED);
			}

			/* Send source cap some minimum number of times */
			if (pd[port].caps_count < PD_CAPS_COUNT) {
				/* Query capabilities of the other side */
				send_source_cap(port, 1);
				return;
			}
			break;
		case PD_STATE_SRC_NEGOCIATE:
			/* wait for a "Request" message */
			if (pd[port].last_state != pd[port].task_state)
				set_state_timeout(port,
						  timestamp_get() +
						  PD_T_SENDER_RESPONSE,
						  PD_STATE_HARD_RESET_SEND);
			break;
		case PD_STATE_SRC_ACCEPTED:
			/* Accept sent, wait for enabling the new voltage */
			if (pd[port].last_state != pd[port].task_state)
				set_state_timeout(
					port,
					timestamp_get() +
					PD_T_SINK_TRANSITION,
					PD_STATE_SRC_POWERED);
			break;
		case PD_STATE_SRC_POWERED:
			/* Switch to the new requested voltage */
			if (pd[port].last_state != pd[port].task_state) {
				pd_transition_voltage(pd[port].requested_idx);
				set_state_timeout(
					port,
					timestamp_get() +
					PD_POWER_SUPPLY_TURN_ON_DELAY,
					PD_STATE_SRC_TRANSITION);
			}
			break;
		case PD_STATE_SRC_TRANSITION:
			/* the voltage output is good, notify the source */
			send_control(port, PD_CTRL_PS_RDY, pd_txdone_sent_ps_rdy, NULL);
			return;
		case PD_STATE_SRC_READY:
			timeout = PD_T_SOURCE_ACTIVITY;

			/*
			 * Don't send any PD traffic if we woke up due to
			 * incoming packet or if VDO response pending to avoid
			 * collisions.
			 */
			if ((evt & PD_EVENT_RX)
#ifdef CONFIG_USB_PD_USE_VDM
					||(pd[port].vdm_state == VDM_STATE_BUSY)
#endif // #ifdef CONFIG_USB_PD_USE_VDM
				)
				break;

			/* Send updated source capabilities to our partner */
			if (pd[port].flags & PD_FLAGS_UPDATE_SRC_CAPS) {
				send_source_cap(port, 2);
				return;
			}

			/* Send get sink cap if haven't received it yet */
			if (!(pd[port].flags & PD_FLAGS_SNK_CAP_RECVD)) {
				if (++pd[port].snk_cap_count <= PD_SNK_CAP_RETRIES) {
					/* Get sink cap to know if dual-role device */
					send_control(port, PD_CTRL_GET_SINK_CAP, pd_txdone_sent_get_sink_cap, NULL);
					return;
				} //else if (debug_level >= 2 &&
				//	   snk_cap_count == PD_SNK_CAP_RETRIES+1) {
				//	CPRINTF("C%d ERR SNK_CAP\n", port);
				//}
			}

			/* Check power role policy, which may trigger a swap */
			if (pd[port].flags & PD_FLAGS_CHECK_PR_ROLE) {
				pd_check_pr_role(port, PD_ROLE_SOURCE,
						 pd[port].flags);
				pd[port].flags &= ~PD_FLAGS_CHECK_PR_ROLE;
				break;
			}

			/* Check data role policy, which may trigger a swap */
			if (pd[port].flags & PD_FLAGS_CHECK_DR_ROLE) {
				pd_check_dr_role(port, pd[port].data_role,
						 pd[port].flags);
				pd[port].flags &= ~PD_FLAGS_CHECK_DR_ROLE;
				break;
			}

			/* Send discovery SVDMs last */
			if (pd[port].data_role == PD_ROLE_DFP &&
			    (pd[port].flags & PD_FLAGS_CHECK_IDENTITY)) {
#ifdef CONFIG_USB_PD_USE_VDM
				pd_send_vdm(port, USB_SID_PD, CMD_DISCOVER_IDENT, NULL, 0);
#endif // #ifdef CONFIG_USB_PD_USE_VDM
				pd[port].flags &= ~PD_FLAGS_CHECK_IDENTITY;
				break;
			}

			if (!(pd[port].flags & PD_FLAGS_PING_ENABLED))
				break;

#ifdef CONFIG_USD_PD_SRC_SEND_PING
			/* Verify that the sink is alive */
			send_control(port, PD_CTRL_PING, pd_txdone_ping_sent, NULL);
			return;
#else
			break;
#endif
		case PD_STATE_SRC_GET_SINK_CAP:
			if (pd[port].last_state != pd[port].task_state)
				set_state_timeout(port,
						  timestamp_get() +
						  PD_T_SENDER_RESPONSE,
						  PD_STATE_SRC_READY);
			break;
#endif // #ifdef CONFIG_USD_PD_FUNC_SRC

#ifdef CONFIG_USB_PD_DR_SWAP
		case PD_STATE_DR_SWAP:
			if (pd[port].last_state != pd[port].task_state) {
				res = send_control(port, PD_CTRL_DR_SWAP);
				if (res < 0) {
					timeout = 10*MSEC;
					/*
					 * If failed to get goodCRC, send
					 * soft reset, otherwise ignore
					 * failure.
					 */
					set_state(port, res == -1 ?
						   PD_STATE_SOFT_RESET :
						   READY_RETURN_STATE(port));
					break;
				}
				/* Wait for accept or reject */
				set_state_timeout(port,
						  timestamp_get() +
						  PD_T_SENDER_RESPONSE,
						  READY_RETURN_STATE(port));
			}
			break;
#endif // #ifdef CONFIG_USB_PD_DR_SWAP

#ifdef CONFIG_USB_PD_DUAL_ROLE
		case PD_STATE_SRC_SWAP_INIT:
			if (pd[port].last_state != pd[port].task_state) {
				res = send_control(port, PD_CTRL_PR_SWAP);
				if (res < 0) {
					timeout = 10*MSEC;
					/*
					 * If failed to get goodCRC, send
					 * soft reset, otherwise ignore
					 * failure.
					 */
					set_state(port, res == -1 ?
						   PD_STATE_SOFT_RESET :
						   PD_STATE_SRC_READY);
					break;
				}
				/* Wait for accept or reject */
				set_state_timeout(port,
						  timestamp_get() +
						  PD_T_SENDER_RESPONSE,
						  PD_STATE_SRC_READY);
			}
			break;
		case PD_STATE_SRC_SWAP_SNK_DISABLE:
			/* Give time for sink to stop drawing current */
			if (pd[port].last_state != pd[port].task_state)
				set_state_timeout(port,
						  timestamp_get() +
						  PD_T_SINK_TRANSITION,
						  PD_STATE_SRC_SWAP_SRC_DISABLE);
			break;
		case PD_STATE_SRC_SWAP_SRC_DISABLE:
			/* Turn power off */
			if (pd[port].last_state != pd[port].task_state) {
				pd_power_supply_reset(port);
				set_state_timeout(port,
						  timestamp_get() +
						  PD_POWER_SUPPLY_TURN_OFF_DELAY,
						  PD_STATE_SRC_SWAP_STANDBY);
			}
			break;
		case PD_STATE_SRC_SWAP_STANDBY:
			/* Send PS_RDY to let sink know our power is off */
			if (pd[port].last_state != pd[port].task_state) {
				/* Send PS_RDY */
				res = send_control(port, PD_CTRL_PS_RDY);
				if (res < 0) {
					timeout = 10*MSEC;
					set_state(port,
						  PD_STATE_SRC_DISCONNECTED);
					break;
				}
				/* Switch to Rd and swap roles to sink */
				tcpm_set_cc(port, TYPEC_CC_RD);
				pd[port].power_role = PD_ROLE_SINK;
				/* Wait for PS_RDY from new source */
				set_state_timeout(port,
						  timestamp_get() +
						  PD_T_PS_SOURCE_ON,
						  PD_STATE_SNK_DISCONNECTED);
			}
			break;
#endif // #ifdef CONFIG_USB_PD_DUAL_ROLE

#ifdef CONFIG_USB_PD_FUNC_SNK
		case PD_STATE_SNK_DISCONNECTED:
#ifdef CONFIG_USB_PD_LOW_POWER
			timeout = drp_state != PD_DRP_TOGGLE_ON ? SECOND
								: 10*MSEC;
#else
			timeout = 10*MSEC;
#endif
			tcpm_get_cc(port, &cc1, &cc2);

#ifdef CONFIG_USB_PD_DUAL_ROLE_AUTO_TOGGLE
			/*
			 * Attempt TCPC auto DRP toggle if it is
			 * not already auto toggling and not try.src
			 */
			if (auto_toggle_supported &&
			    !(pd[port].flags & PD_FLAGS_TCPC_DRP_TOGGLE) &&
			    !(pd[port].flags & PD_FLAGS_TRY_SRC) &&
			    (cc1 == TYPEC_CC_VOLT_OPEN &&
			     cc2 == TYPEC_CC_VOLT_OPEN)) {
				set_state(port, PD_STATE_DRP_AUTO_TOGGLE);
				timeout = 2*MSEC;
				break;
			}
#endif

			/* Source connection monitoring */
			if (cc1 != TYPEC_CC_VOLT_OPEN ||
			    cc2 != TYPEC_CC_VOLT_OPEN) {
				pd[port].cc_state = PD_CC_NONE;
				pd[port].hard_reset_count = 0;
				pd[port].new_cc_state = PD_CC_NONE;
				pd[port].cc_debounce = timestamp_get() +
							PD_T_CC_DEBOUNCE;
				set_state(port,
					PD_STATE_SNK_DISCONNECTED_DEBOUNCE);
				timeout = 10*MSEC;
				break;
			}

#ifdef CONFIG_USB_PD_DUAL_ROLE
			/*
			 * If Try.SRC is active and failed to detect a SNK,
			 * then it transitions to TryWait.SNK. Need to prevent
			 * normal dual role toggle until tDRPTryWait timer
			 * expires.
			 */
			if (pd[port].flags & PD_FLAGS_TRY_SRC) {
				if (timestamp_get() > pd[port].try_src_marker)
					pd[port].flags &= ~PD_FLAGS_TRY_SRC;
				break;
			}

			/* If no source detected, check for role toggle. */
			if (drp_state == PD_DRP_TOGGLE_ON &&
			    timestamp_get() >= pd[port].next_role_swap) {
				/* Swap roles to source */
				pd[port].power_role = PD_ROLE_SOURCE;
				set_state(port, PD_STATE_SRC_DISCONNECTED);
				tcpm_set_cc(port, TYPEC_CC_RP);
				pd[port].next_role_swap = timestamp_get() + PD_T_DRP_SRC;

				/* Swap states quickly */
				timeout = 2*MSEC;
			}
#endif // #ifdef CONFIG_USB_PD_DUAL_ROLE
			break;
		case PD_STATE_SNK_DISCONNECTED_DEBOUNCE:
			tcpm_get_cc(port, &cc1, &cc2);

			if (cc_is_rp(cc1) && cc_is_rp(cc2)) {
				/* Debug accessory */
				pd[port].new_cc_state = PD_CC_DEBUG_ACC;
			} else if (cc_is_rp(cc1) || cc_is_rp(cc2)) {
				pd[port].new_cc_state = PD_CC_DFP_ATTACHED;
			} else {
				/* No connection any more */
				set_state(port, PD_STATE_SNK_DISCONNECTED);
				timeout = 5*MSEC;
				break;
			}

			timeout = 20*MSEC;

			/* Debounce the cc state */
			if (pd[port].new_cc_state != pd[port].cc_state) {
				pd[port].cc_debounce = timestamp_get() +
					PD_T_CC_DEBOUNCE;
				pd[port].cc_state = pd[port].new_cc_state;
				break;
			}
			/* Wait for CC debounce and VBUS present */
			if (timestamp_get() < pd[port].cc_debounce ||
			    !pd_is_vbus_present(port))
				break;

#ifdef CONFIG_USB_PD_DUAL_ROLE
			if (pd_try_src_enable &&
			    !(pd[port].flags & PD_FLAGS_TRY_SRC)) {
				/*
				 * If TRY_SRC is enabled, but not active,
				 * then force attempt to connect as source.
				 */
				pd[port].try_src_marker = timestamp_get()
					+ PD_T_TRY_SRC;
				/* Swap roles to source */
				pd[port].power_role = PD_ROLE_SOURCE;
				tcpm_set_cc(port, TYPEC_CC_RP);
				timeout = 2*MSEC;
				set_state(port, PD_STATE_SRC_DISCONNECTED);
				/* Set flag after the state change */
				pd[port].flags |= PD_FLAGS_TRY_SRC;
				break;
			}
#endif // #ifdef CONFIG_USB_PD_DUAL_ROLE

			/* We are attached */
			pd[port].polarity = get_snk_polarity(cc1, cc2);
			set_polarity(port, pd[port].polarity);
			/* reset message ID  on connection */
			pd[port].msg_id = 0;
			/* initial data role for sink is UFP */
			pd_set_data_role(port, PD_ROLE_UFP);
#if defined(CONFIG_CHARGE_MANAGER)
			typec_curr = get_typec_current_limit(pd[port].polarity,
							     cc1, cc2);
			typec_set_input_current_limit(
				port, typec_curr, TYPE_C_VOLTAGE);
#endif
			/* If PD comm is enabled, enable TCPC RX */
			if (pd_comm_is_enabled(port))
				tcpm_set_rx_enable(port, 1);

			/* DFP is attached */
			if (pd[port].new_cc_state == PD_CC_DFP_ATTACHED ||
					pd[port].new_cc_state == PD_CC_DEBUG_ACC) {
				pd[port].flags |= PD_FLAGS_CHECK_PR_ROLE |
						  PD_FLAGS_CHECK_DR_ROLE |
						  PD_FLAGS_CHECK_IDENTITY;
				if (pd[port].new_cc_state == PD_CC_DEBUG_ACC)
					pd[port].flags |=
						PD_FLAGS_TS_DTS_PARTNER;
				set_state(port, PD_STATE_SNK_DISCOVERY);
				timeout = 10*MSEC;
			}
			break;
		case PD_STATE_SNK_HARD_RESET_RECOVER:
			if (pd[port].last_state != pd[port].task_state)
				pd[port].flags |= PD_FLAGS_CHECK_IDENTITY;
#ifdef CONFIG_USB_PD_VBUS_DETECT_NONE
			/*
			 * Can't measure vbus state so this is the maximum
			 * recovery time for the source.
			 */
			if (pd[port].last_state != pd[port].task_state)
				set_state_timeout(port, timestamp_get() +
						  PD_T_SAFE_0V +
						  PD_T_SRC_RECOVER_MAX +
						  PD_T_SRC_TURN_ON,
						  PD_STATE_SNK_DISCONNECTED);
#else
			/* Wait for VBUS to go low and then high*/
			if (pd[port].last_state != pd[port].task_state) {
				pd[port].snk_hard_reset_vbus_off = 0;
				set_state_timeout(port,
						  timestamp_get() +
						  PD_T_SAFE_0V,
						  pd[port].hard_reset_count <
						    PD_HARD_RESET_COUNT ?
						     PD_STATE_HARD_RESET_SEND :
						     PD_STATE_SNK_DISCOVERY);
			}

			if (!pd_is_vbus_present(port) &&
			    !pd[port].snk_hard_reset_vbus_off) {
				/* VBUS has gone low, reset timeout */
				pd[port].snk_hard_reset_vbus_off = 1;
				set_state_timeout(port,
						  timestamp_get() +
						  PD_T_SRC_RECOVER_MAX +
						  PD_T_SRC_TURN_ON,
						  PD_STATE_SNK_DISCONNECTED);
			}
			if (pd_is_vbus_present(port) &&
					pd[port].snk_hard_reset_vbus_off) {
#ifdef CONFIG_USB_PD_TCPM_TCPCI
				/*
				 * After transmitting hard reset, TCPM writes
				 * to RECEIVE_MESSAGE register to enable
				 * PD message passing.
				 */
				if (pd_comm_is_enabled(port))
					tcpm_set_rx_enable(port, 1);
#endif /* CONFIG_USB_PD_TCPM_TCPCI */

				/* VBUS went high again */
				set_state(port, PD_STATE_SNK_DISCOVERY);
				timeout = 10*MSEC;
			}

			/*
			 * Don't need to set timeout because VBUS changing
			 * will trigger an interrupt and wake us up.
			 */
#endif
			break;
		case PD_STATE_SNK_DISCOVERY:
			/* Wait for source cap expired only if we are enabled */
			if ((pd[port].last_state != pd[port].task_state)
			    && pd_comm_is_enabled(port)) {
				/*
				 * If VBUS has never been low, and we timeout
				 * waiting for source cap, try a soft reset
				 * first, in case we were already in a stable
				 * contract before this boot.
				 */
				if (pd[port].flags & PD_FLAGS_VBUS_NEVER_LOW)
					set_state_timeout(port,
						  timestamp_get() +
						  PD_T_SINK_WAIT_CAP,
						  PD_STATE_SOFT_RESET);
				/*
				 * If we haven't passed hard reset counter,
				 * start SinkWaitCapTimer, otherwise start
				 * NoResponseTimer.
				 */
				else if (pd[port].hard_reset_count < PD_HARD_RESET_COUNT)
					set_state_timeout(port,
						  timestamp_get() +
						  PD_T_SINK_WAIT_CAP,
						  PD_STATE_HARD_RESET_SEND);
				else if (pd_capable(port))
					/* ErrorRecovery */
					set_state_timeout(port,
						  timestamp_get() +
						  PD_T_NO_RESPONSE,
						  PD_STATE_SNK_DISCONNECTED);
				else {
					// Rikka's patch: source is not PD capable
					set_state(port, PD_STATE_SNK_READY);
					timeout = 5*MSEC;
				}
#if defined(CONFIG_CHARGE_MANAGER)
				/*
				 * If we didn't come from disconnected, must
				 * have come from some path that did not set
				 * typec current limit. So, set to 0 so that
				 * we guarantee this is revised below.
				 */
				if (pd[port].last_state !=
				    PD_STATE_SNK_DISCONNECTED_DEBOUNCE)
					typec_curr = 0;
#endif
			}

#if defined(CONFIG_CHARGE_MANAGER)
			timeout = PD_T_SINK_ADJ - PD_T_DEBOUNCE;

			/* Check if CC pull-up has changed */
			tcpm_get_cc(port, &cc1, &cc2);
			if (typec_curr != get_typec_current_limit(
						pd[port].polarity, cc1, cc2)) {
				/* debounce signal by requiring two reads */
				if (typec_curr_change) {
					/* set new input current limit */
					typec_curr = get_typec_current_limit(
						pd[port].polarity, cc1, cc2);
					typec_set_input_current_limit(
					  port, typec_curr, TYPE_C_VOLTAGE);
				} else {
					/* delay for debounce */
					timeout = PD_T_DEBOUNCE;
				}
				typec_curr_change = !typec_curr_change;
			} else {
				typec_curr_change = 0;
			}
#endif
			break;
		case PD_STATE_SNK_REQUESTED:
			/* Wait for ACCEPT or REJECT */
			if (pd[port].last_state != pd[port].task_state) {
				pd[port].hard_reset_count = 0;
				set_state_timeout(port,
						  timestamp_get() +
						  PD_T_SENDER_RESPONSE,
						  PD_STATE_HARD_RESET_SEND);
			}
			break;
		case PD_STATE_SNK_TRANSITION:
			/* Wait for PS_RDY */
			if (pd[port].last_state != pd[port].task_state)
				set_state_timeout(port,
						  timestamp_get() +
						  PD_T_PS_TRANSITION,
						  PD_STATE_HARD_RESET_SEND);
			break;
		case PD_STATE_SNK_READY:
			timeout = 20*MSEC;
			if (!pd_capable(port)) {
				timeout = 200*MSEC;
				break;
			}

			/*
			 * Don't send any PD traffic if we woke up due to
			 * incoming packet or if VDO response pending to avoid
			 * collisions.
			 */
			if ((evt & PD_EVENT_RX)
#ifdef CONFIG_USB_PD_USE_VDM
				|| (pd[port].vdm_state == VDM_STATE_BUSY)
#endif // #ifdef CONFIG_USB_PD_USE_VDM
				)
				break;

			/* Check for new power to request */
			if (pd[port].new_power_request) {
				switch(pd_send_request_msg(port, 0)) {
				case 1:		// Changed & new request sent
					return;
				case -1:	// pd_build_request() Failed
					set_state(port, PD_STATE_SOFT_RESET);
				default:	// No change
					break;
				}
				break;
			}

			/* Check power role policy, which may trigger a swap */
			if (pd[port].flags & PD_FLAGS_CHECK_PR_ROLE) {
				pd_check_pr_role(port, PD_ROLE_SINK,
						 pd[port].flags);
				pd[port].flags &= ~PD_FLAGS_CHECK_PR_ROLE;
				break;
			}

			/* Check data role policy, which may trigger a swap */
			if (pd[port].flags & PD_FLAGS_CHECK_DR_ROLE) {
				pd_check_dr_role(port, pd[port].data_role,
						 pd[port].flags);
				pd[port].flags &= ~PD_FLAGS_CHECK_DR_ROLE;
				break;
			}

			/* If DFP, send discovery SVDMs */
			if (pd[port].data_role == PD_ROLE_DFP &&
			     (pd[port].flags & PD_FLAGS_CHECK_IDENTITY)) {
#ifdef CONFIG_USB_PD_USE_VDM
				pd_send_vdm(port, USB_SID_PD, CMD_DISCOVER_IDENT, NULL, 0);
#endif // #ifdef CONFIG_USB_PD_USE_VDM
				pd[port].flags &= ~PD_FLAGS_CHECK_IDENTITY;
				break;
			}

			/* Sent all messages, don't need to wake very often */
			timeout = 200*MSEC;
			break;
#endif // #ifdef CONFIG_USB_PD_FUNC_SNK

#ifdef CONFIG_USB_PD_DUAL_ROLE
		case PD_STATE_SNK_SWAP_INIT:
			if (pd[port].last_state != pd[port].task_state) {
				res = send_control(port, PD_CTRL_PR_SWAP);
				if (res < 0) {
					timeout = 10*MSEC;
					/*
					 * If failed to get goodCRC, send
					 * soft reset, otherwise ignore
					 * failure.
					 */
					set_state(port, res == -1 ?
						   PD_STATE_SOFT_RESET :
						   PD_STATE_SNK_READY);
					break;
				}
				/* Wait for accept or reject */
				set_state_timeout(port,
						  timestamp_get() +
						  PD_T_SENDER_RESPONSE,
						  PD_STATE_SNK_READY);
			}
			break;
		case PD_STATE_SNK_SWAP_SNK_DISABLE:
			/* Stop drawing power */
			pd_set_input_current_limit(port, 0, 0);
#ifdef CONFIG_CHARGE_MANAGER
			typec_set_input_current_limit(port, 0, 0);
			charge_manager_set_ceil(port,
						CEIL_REQUESTOR_PD,
						CHARGE_CEIL_NONE);
#endif
			set_state(port, PD_STATE_SNK_SWAP_SRC_DISABLE);
			timeout = 10*MSEC;
			break;
		case PD_STATE_SNK_SWAP_SRC_DISABLE:
			/* Wait for PS_RDY */
			if (pd[port].last_state != pd[port].task_state)
				set_state_timeout(port,
						  timestamp_get() +
						  PD_T_PS_SOURCE_OFF,
						  PD_STATE_HARD_RESET_SEND);
			break;
		case PD_STATE_SNK_SWAP_STANDBY:
			if (pd[port].last_state != pd[port].task_state) {
				/* Switch to Rp and enable power supply */
				tcpm_set_cc(port, TYPEC_CC_RP);
				if (pd_set_power_supply_ready(port)) {
					/* Restore Rd */
					tcpm_set_cc(port, TYPEC_CC_RD);
					timeout = 10*MSEC;
					set_state(port,
						  PD_STATE_SNK_DISCONNECTED);
					break;
				}
				/* Wait for power supply to turn on */
				set_state_timeout(
					port,
					timestamp_get() +
					PD_POWER_SUPPLY_TURN_ON_DELAY,
					PD_STATE_SNK_SWAP_COMPLETE);
			}
			break;
		case PD_STATE_SNK_SWAP_COMPLETE:
			/* Send PS_RDY and change to source role */
			res = send_control(port, PD_CTRL_PS_RDY);
			if (res < 0) {
				/* Restore Rd */
				tcpm_set_cc(port, TYPEC_CC_RD);
				pd_power_supply_reset(port);
				timeout = 10 * MSEC;
				set_state(port, PD_STATE_SNK_DISCONNECTED);
				break;
			}

			/* Don't send GET_SINK_CAP on swap */
			pd[port].snk_cap_count = PD_SNK_CAP_RETRIES+1;
			pd[port].caps_count = 0;
			pd[port].msg_id = 0;
			pd[port].power_role = PD_ROLE_SOURCE;
			pd_update_roles(port);
			set_state(port, PD_STATE_SRC_DISCOVERY);
			timeout = 10*MSEC;
			break;
#ifdef CONFIG_USBC_VCONN_SWAP
		case PD_STATE_VCONN_SWAP_SEND:
			if (pd[port].last_state != pd[port].task_state) {
				res = send_control(port, PD_CTRL_VCONN_SWAP);
				if (res < 0) {
					timeout = 10*MSEC;
					/*
					 * If failed to get goodCRC, send
					 * soft reset, otherwise ignore
					 * failure.
					 */
					set_state(port, res == -1 ?
						   PD_STATE_SOFT_RESET :
						   READY_RETURN_STATE(port));
					break;
				}
				/* Wait for accept or reject */
				set_state_timeout(port,
						  timestamp_get() +
						  PD_T_SENDER_RESPONSE,
						  READY_RETURN_STATE(port));
			}
			break;
		case PD_STATE_VCONN_SWAP_INIT:
			if (pd[port].last_state != pd[port].task_state) {
				if (!(pd[port].flags & PD_FLAGS_VCONN_ON)) {
					/* Turn VCONN on and wait for it */
					set_vconn(port, 1);
					set_state_timeout(port,
					  timestamp_get() + PD_VCONN_SWAP_DELAY,
					  PD_STATE_VCONN_SWAP_READY);
				} else {
					set_state_timeout(port,
					  timestamp_get() + PD_T_VCONN_SOURCE_ON,
					  READY_RETURN_STATE(port));
				}
			}
			break;
		case PD_STATE_VCONN_SWAP_READY:
			if (pd[port].last_state != pd[port].task_state) {
				if (!(pd[port].flags & PD_FLAGS_VCONN_ON)) {
					/* VCONN is now on, send PS_RDY */
					pd[port].flags |= PD_FLAGS_VCONN_ON;
					res = send_control(port,
							   PD_CTRL_PS_RDY);
					if (res == -1) {
						timeout = 10*MSEC;
						/*
						 * If failed to get goodCRC,
						 * send soft reset
						 */
						set_state(port,
							  PD_STATE_SOFT_RESET);
						break;
					}
					set_state(port,
						  READY_RETURN_STATE(port));
				} else {
					/* Turn VCONN off and wait for it */
					set_vconn(port, 0);
					pd[port].flags &= ~PD_FLAGS_VCONN_ON;
					set_state_timeout(port,
					  timestamp_get() + PD_VCONN_SWAP_DELAY,
					  READY_RETURN_STATE(port));
				}
			}
			break;
#endif /* CONFIG_USBC_VCONN_SWAP */
#endif /* CONFIG_USB_PD_DUAL_ROLE */
		case PD_STATE_SOFT_RESET:
			if (pd[port].last_state != pd[port].task_state) {
				/* Message ID of soft reset is always 0 */
				pd[port].msg_id = 0;
				send_control(port, PD_CTRL_SOFT_RESET, pd_txdone_sent_srst, 0);
				return;
			}
			break;
		case PD_STATE_HARD_RESET_SEND:
			pd[port].hard_reset_count++;
			if (pd[port].last_state != pd[port].task_state)
				pd[port].hard_reset_sent = 0;
#ifdef CONFIG_CHARGE_MANAGER
			if (pd[port].last_state == PD_STATE_SNK_DISCOVERY ||
			    (pd[port].last_state == PD_STATE_SOFT_RESET &&
			     (pd[port].flags & PD_FLAGS_VBUS_NEVER_LOW))) {
				pd[port].flags &= ~PD_FLAGS_VBUS_NEVER_LOW;
				/*
				 * If discovery timed out, assume that we
				 * have a dedicated charger attached. This
				 * may not be a correct assumption according
				 * to the specification, but it generally
				 * works in practice and the harmful
				 * effects of a wrong assumption here
				 * are minimal.
				 */
				charge_manager_update_dualrole(port,
							       CAP_DEDICATED);
			}
#endif

			/* try sending hard reset until it succeeds */
			if (!pd[port].hard_reset_sent) {
				pd_transmit(port, TCPC_TX_HARD_RESET, 0, NULL, pd_txdone_sent_hrst, 0);
				return;
			}
			break;
		case PD_STATE_HARD_RESET_EXECUTE:
#ifdef CONFIG_USB_PD_DUAL_ROLE
			/*
			 * If hard reset while in the last stages of power
			 * swap, then we need to restore our CC resistor.
			 */
			if (pd[port].last_state == PD_STATE_SNK_SWAP_STANDBY)
				tcpm_set_cc(port, TYPEC_CC_RD);
#endif

			/* reset our own state machine */
			pd_execute_hard_reset(port);
			timeout = 10*MSEC;
			break;
#ifdef CONFIG_USB_PD_DUAL_ROLE_AUTO_TOGGLE
		case PD_STATE_DRP_AUTO_TOGGLE:
		{
			enum pd_states next_state;

			/* Check for connection */
			tcpm_get_cc(port, &cc1, &cc2);

			/* Set to appropriate port state */
			if (cc1 == TYPEC_CC_VOLT_OPEN &&
			    cc2 == TYPEC_CC_VOLT_OPEN)
				/* nothing connected, keep toggling*/
				next_state = PD_STATE_DRP_AUTO_TOGGLE;
			else if ((cc_is_rp(cc1) || cc_is_rp(cc2)) &&
				 drp_state != PD_DRP_FORCE_SOURCE)
				/* SNK allowed unless ForceSRC */
				next_state = PD_STATE_SNK_DISCONNECTED;
			else if (((cc1 == TYPEC_CC_VOLT_RD ||
				   cc2 == TYPEC_CC_VOLT_RD) ||
				  (cc1 == TYPEC_CC_VOLT_RA &&
				   cc2 == TYPEC_CC_VOLT_RA)) &&
				 (drp_state != PD_DRP_TOGGLE_OFF &&
				  drp_state != PD_DRP_FORCE_SINK))
				/* SRC allowed unless ForceSNK or Toggle Off */
				next_state = PD_STATE_SRC_DISCONNECTED;
			else
				/* Anything else, keep toggling */
				next_state = PD_STATE_DRP_AUTO_TOGGLE;

			if (next_state != PD_STATE_DRP_AUTO_TOGGLE) {
				tcpm_set_drp_toggle(port, 0);
#ifdef CONFIG_USB_PD_TCPC_LOW_POWER
				//CPRINTS("TCPC p%d Exit Low Power Mode", port);
#endif
			}

			if (next_state == PD_STATE_SNK_DISCONNECTED) {
				tcpm_set_cc(port, TYPEC_CC_RD);
				pd[port].power_role = PD_ROLE_SINK;
				timeout = 2*MSEC;
			} else if (next_state == PD_STATE_SRC_DISCONNECTED) {
				tcpm_set_cc(port, TYPEC_CC_RP);
				pd[port].power_role = PD_ROLE_SOURCE;
				timeout = 2*MSEC;
			} else {
				tcpm_set_drp_toggle(port, 1);
				pd[port].flags |= PD_FLAGS_TCPC_DRP_TOGGLE;
				timeout = -1;
#ifdef CONFIG_USB_PD_TCPC_LOW_POWER
				//CPRINTS("TCPC p%d Low Power Mode", port);
#endif
			}
			set_state(port, next_state);

			break;
		}
#endif
		default:
			break;
		}

		pd[port].last_state = this_state;
EOS_STATE_MACHINE:
		/*
		 * Check for state timeout, and if not check if need to adjust
		 * timeout value to wake up on the next state timeout.
		 */
		now = timestamp_get();
		if (pd[port].timeout) {
			if (now >= pd[port].timeout) {
				set_state(port, pd[port].timeout_state);
				/* On a state timeout, run next state soon */
				timeout = timeout < 10*MSEC ? timeout : 10*MSEC;
			} else if (pd[port].timeout - now < timeout) {
				timeout = pd[port].timeout - now;
			}
		}
		pd[port].next_protocol_run = now + timeout;

		/* Check for disconnection if we're connected */
		if (!pd_is_connected(port))
			return;
#ifdef CONFIG_USB_PD_DUAL_ROLE
		if (pd_is_power_swapping(port))
			return;
#endif

#ifdef CONFIG_USD_PD_FUNC_SRC
		if (pd[port].power_role == PD_ROLE_SOURCE) {
			/* Source: detect disconnect by monitoring CC */
			tcpm_get_cc(port, &cc1, &cc2);
			if (pd[port].polarity)
				cc1 = cc2;
			if (cc1 == TYPEC_CC_VOLT_OPEN) {
				set_state(port, PD_STATE_SRC_DISCONNECTED);
				/* Debouncing */
				timeout = 10*MSEC;
#ifdef CONFIG_USB_PD_DUAL_ROLE
				/*
				 * If Try.SRC is configured, then ATTACHED_SRC
				 * needs to transition to TryWait.SNK. Change
				 * power role to SNK and start state timer.
				 */
				if (pd_try_src_enable) {
					/* Swap roles to sink */
					pd[port].power_role = PD_ROLE_SINK;
					tcpm_set_cc(port, TYPEC_CC_RD);
					/* Set timer for TryWait.SNK state */
					pd[port].try_src_marker = timestamp_get()
						+ PD_T_TRY_WAIT;
					/* Advance to TryWait.SNK state */
					set_state(port,
						  PD_STATE_SNK_DISCONNECTED);
					/* Mark state as TryWait.SNK */
					pd[port].flags |= PD_FLAGS_TRY_SRC;
				}
#endif
			}
		}
#endif // #ifdef CONFIG_USD_PD_FUNC_SRC

#ifdef CONFIG_USB_PD_FUNC_SNK
		/*
		 * Sink disconnect if VBUS is low and we are not recovering
		 * a hard reset.
		 */
		if (pd[port].power_role == PD_ROLE_SINK &&
		    !pd_is_vbus_present(port) &&
		    pd[port].task_state != PD_STATE_SNK_HARD_RESET_RECOVER &&
		    pd[port].task_state != PD_STATE_HARD_RESET_EXECUTE) {
			/* Sink: detect disconnect by monitoring VBUS */
			set_state(port, PD_STATE_SNK_DISCONNECTED);
			/* set timeout small to reconnect fast */
			timeout = 5*MSEC;
		}
#endif /* CONFIG_USB_PD_FUNC_SNK */
}

#ifdef CONFIG_COMMON_RUNTIME
int pd_is_port_enabled(int port)
{
	switch (pd[port].task_state) {
	case PD_STATE_DISABLED:
	case PD_STATE_SUSPENDED:
		return 0;
	default:
		return 1;
	}
}

#if defined(CONFIG_USB_PD_ALT_MODE) && !defined(CONFIG_USB_PD_ALT_MODE_DFP)
void pd_send_hpd(int port, enum hpd_event hpd)
{
	uint32_t data[1];
	int opos = pd_alt_mode(port, USB_SID_DISPLAYPORT);
	if (!opos)
		return;

	data[0] = VDO_DP_STATUS((hpd == hpd_irq),  /* IRQ_HPD */
				(hpd != hpd_low),  /* HPD_HI|LOW */
				0,		      /* request exit DP */
				0,		      /* request exit USB */
				0,		      /* MF pref */
				1,                    /* enabled */
				0,		      /* power low */
				0x2);
	pd_send_vdm(port, USB_SID_DISPLAYPORT,
		    VDO_OPOS(opos) | CMD_ATTENTION, data, 1);
	/* Wait until VDM is done. */
	while (pd[0].vdm_state > 0)
		task_wait_event(USB_PD_RX_TMOUT_US * (PD_RETRY_COUNT + 1));
}
#endif


#ifdef CONFIG_USB_PD_DUAL_ROLE
void pd_request_source_voltage(int port, int mv)
{
	pd_set_max_voltage(mv);

	if (pd[port].task_state == PD_STATE_SNK_READY ||
	    pd[port].task_state == PD_STATE_SNK_TRANSITION) {
		/* Set flag to send new power request in pd_task */
		pd[port].new_power_request = 1;
	} else {
		pd[port].power_role = PD_ROLE_SINK;
		tcpm_set_cc(port, TYPEC_CC_RD);
		set_state(port, PD_STATE_SNK_DISCONNECTED);
	}

	task_wake(PD_PORT_TO_TASK_ID(port));
}

void pd_set_external_voltage_limit(int port, int mv)
{
	pd_set_max_voltage(mv);

	if (pd[port].task_state == PD_STATE_SNK_READY ||
	    pd[port].task_state == PD_STATE_SNK_TRANSITION) {
		/* Set flag to send new power request in pd_task */
		pd[port].new_power_request = 1;
		task_wake(PD_PORT_TO_TASK_ID(port));
	}
}

void pd_update_contract(int port)
{
	if ((pd[port].task_state >= PD_STATE_SRC_NEGOCIATE) &&
	    (pd[port].task_state <= PD_STATE_SRC_GET_SINK_CAP)) {
		pd[port].flags |= PD_FLAGS_UPDATE_SRC_CAPS;
		task_wake(PD_PORT_TO_TASK_ID(port));
	}
}

#endif /* CONFIG_USB_PD_DUAL_ROLE */

#endif /* CONFIG_COMMON_RUNTIME */
