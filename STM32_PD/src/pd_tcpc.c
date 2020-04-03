#include "platform.h"
#include "pd_phy.h"
#include "tcpci.h"
#include "pd.h"	// for GoodCRC and headers

#define TCPC_TIMER_TOGGLE_SNK 60000
#define TCPC_TIMER_TOGGLE_SRC 60000
#define TCPC_TIMER_INTERFRAME_GAP 30	// uS

// internal_flags
#define TCPC_FLAG_LOOKING4CON (1<<0)
#define TCPC_FLAG_DRP_TOGGLE_AS_SNK (1<<1)
#define TCPC_FLAG_DRP_TOGGLE_AS_SRC (1<<2)
#define TCPC_FLAG_CON_RESULT (1<<3)
#define TCPC_FLAG_DEBOUNCING (1<<4)
#define TCPC_FLAG_INT_ASSERTED (1<<5)
#define TCPC_FLAG_TX_PENDING (1<<6)

#define RX_BUFFER_SIZE 1

static struct pd_port_controller {
	/* Port polarity : 0 => CC1 is CC line, 1 => CC2 is CC line */
	uint8_t cc_tcpc_ctrl;	// TCPC_Control bit0
	uint8_t cc_power_ctrl;
	uint8_t cc_role_ctrl;
	/* CC status */
	uint8_t cc_status[2];	// tcpc_cc_voltage_status

	/* TCPC alert status */
	uint16_t alert;
	uint16_t alert_mask;
	/* RX enabled */
	uint8_t reg_recv_detect;
	/* Power status */
	uint8_t power_status;
	uint8_t power_status_mask;
	uint16_t vbus_100mv;

	// MESSAGE_HEADER_INFO
	uint8_t reg_msg_header_info;

	/* Last received */
	pd_message rx_message[RX_BUFFER_SIZE];
	// Tail points to the last added
	int rx_buf_count, rx_buf_tail;

	/* Next transmit */
	enum tcpm_transmit_type tx_type;
	int8_t tx_retry_count;
	uint8_t tx_payload[30];
	uint64_t tx_start_timestamp;

	/* Internal Flags */
	uint32_t internal_flags;
	uint64_t drp_last_toggle_timestamp;
	uint64_t cc_debouncing_timestamp;
	uint64_t cc_last_sampled_timestamp;
} tcpc;

#define TX_LEN(header) (2+(PD_HEADER_CNT(header)<<2))	// 2-byte header + 4-byte payloads

static inline void tx_buf_clear(void)
{
	for (uint8_t i = 0; i < sizeof(tcpc.tx_payload); i++)
		tcpc.tx_payload[i] = 0;
	tcpc.internal_flags &=~ TCPC_FLAG_TX_PENDING;
}

static inline void rx_buf_put(uint8_t frame_type)
{
	uint8_t rx_buf_head = tcpc.rx_buf_tail + tcpc.rx_buf_count;

	if (rx_buf_head >= RX_BUFFER_SIZE)
		rx_buf_head -= RX_BUFFER_SIZE;

	tcpc.rx_message[rx_buf_head].frame_type = frame_type;
	tcpc.rx_message[rx_buf_head].header =
			pd_phy_get_rx_msg((uint8_t*)(&tcpc.rx_message[rx_buf_head].payload));

	tcpc.rx_buf_count++;
}

static void alert(uint16_t mask)
{
	/* Always update the Alert status register */
	tcpc.alert |= mask;
	/*
	 * Only send interrupt to TCPM if corresponding
	 * bit in the alert_enable register is set.
	 */
	if (tcpc.alert_mask & mask) {
		tcpc.internal_flags |= TCPC_FLAG_INT_ASSERTED;
		tcpc_alert(0);
	}
}

void tcpc_alert_status_clear(uint16_t mask)
{
	/*
	 * If the RX status alert is attempting to be cleared, then increment
	 * rx buffer tail pointer. if the RX buffer is not empty, then keep
	 * the RX status alert active.
	 */
	if (mask & TCPC_REG_ALERT_RX_STATUS) {
		if (tcpc.rx_buf_count > 0) {
			// Remove the oldest message from buffer
			__asm__ __volatile__("cpsid i");
			tcpc.rx_buf_tail++;
			if (tcpc.rx_buf_tail >= RX_BUFFER_SIZE)
				tcpc.rx_buf_tail -= RX_BUFFER_SIZE;
			tcpc.rx_buf_count--;
			__asm__ __volatile__("cpsie i");
			if (tcpc.rx_buf_count > 0)
				/* buffer is not empty, keep alert active */
				mask &= ~TCPC_REG_ALERT_RX_STATUS;
		}
	}

	/* clear only the bits specified by the TCPM */
	tcpc.alert &= ~mask;

	/* Set Alert# inactive if all alert bits clear */
	if (!tcpc.alert) {
		__asm__ __volatile__("cpsid i");
		tcpc.internal_flags &=~ TCPC_FLAG_INT_ASSERTED;
		__asm__ __volatile__("cpsie i");
	}
}

uint8_t tcpc_get_message(pd_message* msg)
{
	if (msg != 0 && tcpc.rx_buf_count>0) {
		for (uint8_t i=0; i<sizeof(pd_message); i++) {
			((uint8_t*)msg)[i] = ((uint8_t*)&tcpc.rx_message[tcpc.rx_buf_tail])[i];
		}
		return 3 + ((PD_HEADER_CNT(tcpc.rx_message[tcpc.rx_buf_tail].header)) << 2);
	}
	return 0xFF;
}

void tcpc_send_message(const pd_message* msg)
{
	tcpc.tx_type = msg->frame_type;
	uint8_t cnt = PD_HEADER_CNT(msg->header) << 2;
	tcpc.tx_payload[0] = msg->header & 0xFF;
	tcpc.tx_payload[1] = (msg->header >> 8) & 0xFF;
	for (uint8_t i=0; i<cnt; i++)
		tcpc.tx_payload[i+2] = ((uint8_t*)&msg->payload)[i];
	tcpc.internal_flags |= TCPC_FLAG_TX_PENDING;
	tcpc.tx_retry_count = -1;
}

void tcpc_init(void)
{
	tcpc.power_status |= TCPC_REG_POWER_STATUS_UNINIT | TCPC_REG_POWER_STATUS_VBUS_DET;

	// Initialize CC pull-up/pull-down and Vconn controller
	pd_cc_rprp_init();

	// Initialize PD BMC transceiver
	pd_init();

	tcpc.cc_role_ctrl = TCPC_REG_ROLE_CTRL_SET(0, TYPEC_RP_USB, TYPEC_CC_RD, TYPEC_CC_RD);
	tcpc.reg_recv_detect = 0;

	/* set default alert and power mask register values */
	tcpc.alert_mask = TCPC_REG_ALERT_MASK_ALL;
	tcpc.power_status_mask = TCPC_REG_POWER_STATUS_MASK_ALL;

	// Init done!
	tcpc.power_status &=~ TCPC_REG_POWER_STATUS_UNINIT;
	alert(TCPC_REG_ALERT_POWER_STATUS);
}

uint8_t tcpc_is_int_asserted()
{
	return (tcpc.internal_flags&TCPC_FLAG_INT_ASSERTED) ? 1 : 0;
}

void tcpc_look4forconnection(void)
{
	uint8_t cc1 = TCPC_REG_ROLE_CTRL_CC1(tcpc.cc_role_ctrl);
	uint8_t cc2 = TCPC_REG_ROLE_CTRL_CC2(tcpc.cc_role_ctrl);

	if (cc1 != cc2)	// If cc1 and cc2 are not both Rp/Rd at the same time, then return
		return;

	if (cc2 == TYPEC_CC_RP || cc2 == TYPEC_CC_RD)
		tcpc.internal_flags |= TCPC_FLAG_LOOKING4CON;

	if (tcpc.cc_role_ctrl & TCPC_REG_ROLE_CTRL_DRP_MASK) {
		// DRP auto-toggle is enabled
		if (cc2 == TYPEC_CC_RP) {
			tcpc.internal_flags |= TCPC_FLAG_DRP_TOGGLE_AS_SRC;
			tcpc.drp_last_toggle_timestamp = timestamp_get();
		} else if (cc2 == TYPEC_CC_RD) {
			tcpc.internal_flags |= TCPC_FLAG_DRP_TOGGLE_AS_SNK;
			tcpc.drp_last_toggle_timestamp = timestamp_get();
		}
	}
}

void tcpc_role_ctrl_change(uint8_t newval)
{
	tcpc.internal_flags &=~ (TCPC_FLAG_LOOKING4CON | TCPC_FLAG_DRP_TOGGLE_AS_SNK | TCPC_FLAG_DRP_TOGGLE_AS_SRC);

	// Update DRP bit
	tcpc.cc_role_ctrl &= ~ TCPC_REG_ROLE_CTRL_DRP_MASK;
	tcpc.cc_role_ctrl |= newval & TCPC_REG_ROLE_CTRL_DRP_MASK;

	if (TCPC_REG_ROLE_CTRL_CCXRP(tcpc.cc_role_ctrl) == TCPC_REG_ROLE_CTRL_CCXRP(newval))
		return;	// Nothing changed, do not change Rp/Rd

	// Rp/Rd changed

	tcpc.cc_role_ctrl = newval;	// DRP bit has been updated already
	pd_cc_set(TCPC_REG_ROLE_CTRL_CCXRP(tcpc.cc_role_ctrl));

	/*
	 * Before CC pull can be changed and the task can read the new
	 * status, we should set the CC status to open, in case TCPM
	 * asks before it is known for sure.
	 */
	tcpc.cc_status[0] = TYPEC_CC_VOLT_OPEN;
	tcpc.cc_status[1] = TYPEC_CC_VOLT_OPEN;

	tcpc.cc_last_sampled_timestamp = timestamp_get();	// Postponed the next sampling of CC line, debouncing
}

void tcpc_i2c_write(uint8_t reg, uint32_t len, const uint8_t *payload)
{
	uint16_t alert;

	if (len < 1)
		return;

	/* If we are not yet initialized, ignore any write command */
	if (tcpc.power_status & TCPC_REG_POWER_STATUS_UNINIT)
		return;

	switch (reg) {

	// Alert & Masks
	case TCPC_REG_ALERT:	// Write bit1 to clear corresponding bit
		alert = payload[0];
		alert |= (payload[1] << 8);
		/* clear alert bits specified by the TCPM */
		tcpc_alert_status_clear(alert);
		break;
	case TCPC_REG_ALERT_MASK:
		tcpc.alert_mask = payload[0];
		tcpc.alert_mask |= (payload[1] << 8);
		break;
	case TCPC_REG_POWER_STATUS_MASK:
		tcpc.power_status_mask = payload[0];
		break;

	// Control registers
	case TCPC_REG_TCPC_CTRL:
		tcpc.cc_tcpc_ctrl = payload[0];
		break;
	case TCPC_REG_ROLE_CTRL:
		tcpc_role_ctrl_change(payload[0]);
		break;
	case TCPC_REG_POWER_CTRL:
		tcpc.cc_power_ctrl = payload[0];
		pd_set_vconn(TCPC_REG_POWER_CTRL_VCONN(tcpc.cc_power_ctrl), TCPC_REG_TCPC_CTRL_POLARITY(tcpc.cc_tcpc_ctrl));
		break;

	// Command register, write only
	case TCPC_REG_COMMAND:
		if (payload[0] == TCPC_REG_COMMAND_LOOK4CONNECTION) tcpc_look4forconnection();
		break;

	// MESSAGE_HEADER_INFO
	case TCPC_REG_MSG_HDR_INFO:
		__asm__ __volatile__("cpsid i");
		tcpc.reg_msg_header_info = payload[0] & 0x1F;
		__asm__ __volatile__("cpsie i");
		break;
	// RECEIVE_DETECT
	case TCPC_REG_RX_DETECT:
		tcpc.reg_recv_detect = payload[0];

		if (!TCPC_REG_RX_ENABLED(tcpc.reg_recv_detect)) {
			pd_phy_rx_disable_monitoring();
			pd_select_cc(0);
			tx_buf_clear();
		}

		break;


	case TCPC_REG_TRANSMIT:
		tcpc.tx_type = TCPC_REG_TRANSMIT_TYPE(payload[0]);

		tcpc.internal_flags |= TCPC_FLAG_TX_PENDING;
		tcpc.tx_retry_count = -1;
		break;
	case TCPC_REG_TX_BYTE_CNT:
		// tx_len = 2 + HEADER.COUNT * 4
		break;
	case TCPC_REG_TX_HDR:
		tcpc.tx_payload[0] = payload[0];
		tcpc.tx_payload[1] = payload[1];
		break;
	case TCPC_REG_TX_DATA:
		for (uint8_t i=0; i<len; i++)
			tcpc.tx_payload[i+2] = payload[i];
		break;
	}
}

uint32_t tcpc_i2c_read(uint8_t reg, uint8_t *payload)
{
	switch (reg) {

	// Alert & Masks
	case TCPC_REG_ALERT:
		payload[0] = tcpc.alert & 0xff;
		payload[1] = (tcpc.alert >> 8) & 0xff;
		return 2;
	case TCPC_REG_ALERT_MASK:
		payload[0] = tcpc.alert_mask & 0xff;
		payload[1] = (tcpc.alert_mask >> 8) & 0xff;
		return 2;
	case TCPC_REG_POWER_STATUS_MASK:
		payload[0] = tcpc.power_status_mask;
		return 1;

	// Control registers
	case TCPC_REG_TCPC_CTRL:
		payload[0] = tcpc.cc_tcpc_ctrl;
		return 1;
	case TCPC_REG_ROLE_CTRL:
		payload[0] = tcpc.cc_role_ctrl;
		return 1;
	case TCPC_REG_POWER_CTRL:
		payload[0] = tcpc.cc_power_ctrl;
		return 1;

	// Status registers
	case TCPC_REG_CC_STATUS:
		if (tcpc.internal_flags & TCPC_FLAG_LOOKING4CON) {
			payload[0] = TCPC_REG_CC_STATUS_LOOK4CONNECTION_MASK;
		} else {
			payload[0] = TCPC_REG_CC_STATUS_SET(
					// Connection Result
					(tcpc.cc_role_ctrl&TCPC_REG_ROLE_CTRL_DRP_MASK)
					? ((tcpc.internal_flags & TCPC_FLAG_CON_RESULT) ? 1 : 0)
					: ( TCPC_REG_CC_STATUS_CC1(tcpc.cc_role_ctrl) == TYPEC_CC_RD &&
						TCPC_REG_CC_STATUS_CC2(tcpc.cc_role_ctrl) == TYPEC_CC_RD),

					// If ((POWER_CONTROL.EnableVconn=1 and TCPC_CONTROL.PlugOrientation=1))
					// or ROLE_CONTROL.CC1=Ra or ROLE_CONTROL.CC1=open
					(TCPC_REG_POWER_CTRL_VCONN(tcpc.cc_power_ctrl) && TCPC_REG_TCPC_CTRL_POLARITY(tcpc.cc_tcpc_ctrl)) || (
						TCPC_REG_CC_STATUS_CC1(tcpc.cc_role_ctrl) == TYPEC_CC_RA ||
						TCPC_REG_CC_STATUS_CC1(tcpc.cc_role_ctrl) == TYPEC_CC_OPEN) ? 0 : tcpc.cc_status[0] & 0x3,
					// If ((POWER_CONTROL.EnableVconn=1 and TCPC_CONTROL.PlugOrientation=0))
					// or ROLE_CONTROL.CC2=Ra or ROLE_CONTROL.CC2=open
					(TCPC_REG_POWER_CTRL_VCONN(tcpc.cc_power_ctrl) && !TCPC_REG_TCPC_CTRL_POLARITY(tcpc.cc_tcpc_ctrl)) || (
						TCPC_REG_CC_STATUS_CC2(tcpc.cc_role_ctrl) == TYPEC_CC_RA ||
						TCPC_REG_CC_STATUS_CC2(tcpc.cc_role_ctrl) == TYPEC_CC_OPEN) ? 0 : tcpc.cc_status[1] & 0x3);
		}
		return 1;
	case TCPC_REG_POWER_STATUS:
		payload[0] = tcpc.power_status;
		return 1;


	case TCPC_REG_MSG_HDR_INFO:
		payload[0] = tcpc.reg_msg_header_info;
		return 1;
	case TCPC_REG_RX_DETECT:
		payload[0] = tcpc.reg_recv_detect;
		return 1;

	case TCPC_REG_RX_BYTE_CNT:
		payload[0] = 3 + ((PD_HEADER_CNT(tcpc.rx_message[tcpc.rx_buf_tail].header)) << 2);
		return 1;
	case TCPC_REG_RX_BUF_FRAME_TYPE:
		payload[0] = tcpc.rx_message[tcpc.rx_buf_tail].frame_type;
		return 1;
	case TCPC_REG_RX_HDR:
		payload[0] = tcpc.rx_message[tcpc.rx_buf_tail].header & 0xff;
		payload[1] = (tcpc.rx_message[tcpc.rx_buf_tail].header >> 8) & 0xff;
		return 2;
	case TCPC_REG_RX_DATA:
		for (uint8_t i=0; i<sizeof(tcpc.rx_message[tcpc.rx_buf_tail].payload); i++)
			payload[i] = ((uint8_t*)&(tcpc.rx_message[tcpc.rx_buf_tail].payload))[i];
		return sizeof(tcpc.rx_message[tcpc.rx_buf_tail].payload);

	// Vbus
	case TCPC_REG_VBUS_VOLTAGE:
		payload[0] = tcpc.vbus_100mv & 0xff;
		payload[1] = ((tcpc.vbus_100mv >> 8) & 0xff) | 0x800; // 10: VBUS measurement divided by 4
		return 2;

	default:
		return 0;
	}
}

static inline void tcpc_detect_cc_status(uint64_t cur_timestamp)
{
	static uint8_t last_cc1, last_cc2;

	uint8_t cc1 = pd_cc_read_status(1, TCPC_REG_ROLE_CTRL_CC1(tcpc.cc_role_ctrl), TCPC_REG_ROLE_CTRL_RP(tcpc.cc_role_ctrl));
	uint8_t cc2 = pd_cc_read_status(2, TCPC_REG_ROLE_CTRL_CC2(tcpc.cc_role_ctrl), TCPC_REG_ROLE_CTRL_RP(tcpc.cc_role_ctrl));

	if (tcpc.internal_flags & TCPC_FLAG_DEBOUNCING) {
		if (last_cc1 != cc1 || last_cc2 != cc2) {
			// CC lines are still oscillating
			last_cc1 = cc1;
			last_cc2 = cc2;
			tcpc.cc_debouncing_timestamp = cur_timestamp;
		} else {
			// CC lines are not changing
			if (tcpc.internal_flags & TCPC_FLAG_LOOKING4CON) {
				tcpc.internal_flags &=~ TCPC_FLAG_DEBOUNCING;
				tcpc.cc_status[0] = last_cc1;
				tcpc.cc_status[1] = last_cc2;

				if ((tcpc.internal_flags & TCPC_FLAG_DRP_TOGGLE_AS_SNK) ||
							(TCPC_REG_CC_STATUS_CC2(tcpc.cc_role_ctrl)==TYPEC_CC_RD &&
							!(tcpc.internal_flags&TCPC_FLAG_DRP_TOGGLE_AS_SRC) &&
							!(tcpc.internal_flags&TCPC_FLAG_DRP_TOGGLE_AS_SNK))
						) {
					if (cc1 != TYPEC_CC_VOLT_OPEN
							|| cc2 != TYPEC_CC_VOLT_OPEN) {
						// Found potential connection as sink

						// CC.Looking4Connection=0, CC.ConnectResult=1
						tcpc.internal_flags &= ~(TCPC_FLAG_LOOKING4CON
								| TCPC_FLAG_DRP_TOGGLE_AS_SNK
								| TCPC_FLAG_CON_RESULT);
						tcpc.internal_flags |= TCPC_FLAG_CON_RESULT;
						alert(TCPC_REG_ALERT_CC_STATUS);
					}
				} else if ((tcpc.internal_flags & TCPC_FLAG_DRP_TOGGLE_AS_SRC) ||
							(TCPC_REG_CC_STATUS_CC2(tcpc.cc_role_ctrl)==TYPEC_CC_RP &&
							!(tcpc.internal_flags&TCPC_FLAG_DRP_TOGGLE_AS_SRC) &&
							!(tcpc.internal_flags&TCPC_FLAG_DRP_TOGGLE_AS_SNK))
						) {
					if (cc1 == TYPEC_CC_RD || cc2 == TYPEC_CC_RD
							|| (cc1 == TYPEC_CC_RA && cc2 == TYPEC_CC_RA)) {
						// Found potential connection as source

						// CC.Looking4Connection=0, CC.ConnectResult=0
						tcpc.internal_flags &= ~(TCPC_FLAG_LOOKING4CON
								| TCPC_FLAG_DRP_TOGGLE_AS_SRC
								| TCPC_FLAG_CON_RESULT);
						alert(TCPC_REG_ALERT_CC_STATUS);
					}
				} // There should not be other cases!
			} else {
				if (cur_timestamp > tcpc.cc_debouncing_timestamp + 10000) {
					tcpc.internal_flags &= ~ TCPC_FLAG_DEBOUNCING;

					// Now cc1 and cc2 contains the valid state
					if (tcpc.cc_status[0] != cc1 || tcpc.cc_status[1] != cc2) {
						// Found next state
						tcpc.cc_status[0] = last_cc1;
						tcpc.cc_status[1] = last_cc2;
						alert(TCPC_REG_ALERT_CC_STATUS);
					}
				}
			}
		}
	} else {
		if (tcpc.cc_status[0] != cc1 || tcpc.cc_status[1] != cc2) {
			// CC lines are different from previous valid state
			last_cc1 = cc1;
			last_cc2 = cc2;
			tcpc.internal_flags |= TCPC_FLAG_DEBOUNCING;
			tcpc.cc_debouncing_timestamp = cur_timestamp;
		} // if (pd.cc_status[0] != cc1 || pd.cc_status[1] != cc2) {
	}
}

uint16_t tcpc_phy_get_goodcrc_header(uint8_t rx_result, uint8_t id)
{
	// True if the packet type is supported
	if (tcpc.rx_buf_count >= RX_BUFFER_SIZE) {
		return 0xFE;
	} else if (rx_result >= PD_RX_SOP && rx_result <= PD_RX_SOP_DBGPP) {
		// Buffer has room
		return PD_HEADER(
				PD_CTRL_GOOD_CRC,
				TCPC_REG_MSG_HDR_INFO_PROLE(tcpc.reg_msg_header_info),
				TCPC_REG_MSG_HDR_INFO_DROLE(tcpc.reg_msg_header_info),
				id,
				0,
				TCPC_REG_MSG_HDR_INFO_REV(tcpc.reg_msg_header_info),
				0);	// GoodCRC cannot be extended
	} else {
		// Received Hard Reset or Cable Reset
		return 0xFF;
	}
}

static inline uint8_t check_goodcrc(uint8_t frame_type)
{
	uint16_t goodcrc_header = pd_phy_get_rx_msg(0);
	uint16_t sent_header = tcpc.tx_payload[0];
	sent_header |= ((uint16_t)tcpc.tx_payload[1]) << 8;

	return (
			frame_type == tcpc.tx_type &&
			PD_HEADER_TYPE(goodcrc_header) == PD_CTRL_GOOD_CRC &&
			PD_HEADER_CNT(goodcrc_header) == 0 &&
			PD_HEADER_ID(goodcrc_header) == PD_HEADER_ID(sent_header)
		) ? 1 : 0;
}

// Run TCPC state machine once
void tcpc_run(void)
{
	uint64_t cur_timestamp = timestamp_get();
	uint8_t phy_rx_result;

	if (TCPC_REG_RX_ENABLED(tcpc.reg_recv_detect)) {
		__asm__ __volatile__("cpsid i");

		phy_rx_result = pd_phy_get_rx_type();
		if (phy_rx_result != PD_RX_IDLE) {
			if (phy_rx_result >= PD_RX_SOP && phy_rx_result <= PD_RX_ERR_CABLE_RESET) {
				if (tcpc.reg_recv_detect&(1<<phy_rx_result)) {
					if ((tcpc.internal_flags&TCPC_FLAG_TX_PENDING) && (phy_rx_result==PD_RX_SOP)) {
						// Before we transmit the pending message, a SOP message arrived
						// Discard any message pending transmission
						tx_buf_clear();
						alert(TCPC_REG_ALERT_TX_DISCARDED);
					}
					/*
					 * If there is space in buffer, then increment head to keep
					 * the message and send goodCRC. If this is a hard reset,
					 * send alert regardless of rx buffer status. Else if there is
					 * no space in buffer, then do not send goodCRC and drop
					 * message.
					 */

					if (phy_rx_result == PD_RX_ERR_HARD_RESET) {
						if (tcpc.reg_recv_detect & TCPC_REG_RX_DETECT_HRST) {
							tcpc.reg_recv_detect = 0;
							alert(TCPC_REG_ALERT_RX_HARD_RST);
						}
					} else if (phy_rx_result == PD_RX_ERR_CABLE_RESET) {
						if (tcpc.reg_recv_detect & TCPC_REG_RX_DETECT_CRST) {
							alert(TCPC_REG_ALERT_RX_STATUS);

							tcpc.reg_recv_detect = 0;
							pd_phy_rx_disable_monitoring();
						}
					} else if (tcpc.rx_buf_count < RX_BUFFER_SIZE){
						// Buffer is not full
						rx_buf_put(phy_rx_result);

						alert(TCPC_REG_ALERT_RX_STATUS);
					}
				} // if (pd.reg_recv_detect&(1<<phy_rx_result)) {
			} else if (phy_rx_result == PD_RX_ERR_OVERRUN) {
				alert(TCPC_REG_ALERT_RX_STATUS);
			} else if (phy_rx_result >= PD_RX_SOP_GOODCRC && phy_rx_result <= PD_RX_SOP_DBGPP_GOODCRC &&
					(tcpc.internal_flags&TCPC_FLAG_TX_PENDING) ) {
				// TX, Received GoodCRC

				if (check_goodcrc(phy_rx_result - PD_RX_SOP_GOODCRC)) {
					tx_buf_clear();
					alert(TCPC_REG_ALERT_TX_SUCCESS);
				}
			}

			pd_phy_clear_rx_type();
		} // if (phy_rx_result != PD_RX_IDLE) {

		if (!pd_phy_is_txing()) {
			pd_select_cc(TCPC_REG_TCPC_CTRL_POLARITY(tcpc.cc_tcpc_ctrl) ? 2 : 1);
			pd_phy_rx_enable_monitoring();
		}
		__asm__ __volatile__("cpsie i");
	}

	if (TCPC_REG_RX_ENABLED(tcpc.reg_recv_detect) && (tcpc.internal_flags&TCPC_FLAG_TX_PENDING)) {
		if (pd_phy_is_txing()) {
			// Use drp toggle timestamp variable to ensure the interframe gap
			tcpc.drp_last_toggle_timestamp = cur_timestamp + TCPC_TIMER_INTERFRAME_GAP;
			return;
		} else if (cur_timestamp < tcpc.drp_last_toggle_timestamp) {
			return;
		}

		// We have an unread received message
		if ((tcpc.alert&TCPC_REG_ALERT_RX_STATUS)||(tcpc.alert&TCPC_REG_ALERT_RX_HARD_RST)) {
			tx_buf_clear();
			// Another message received before tx is done
			// Discard the pending tx message
			alert(TCPC_REG_ALERT_TX_DISCARDED);
		} else if (tcpc.tx_type == TCPC_TX_HARD_RESET) {
			// Request to send a hard-reset
			pd_phy_prepare_reset(1);
			pd_tx(0);
			tx_buf_clear();	// No retry
			alert(TCPC_REG_ALERT_TX_SUCCESS|TCPC_REG_ALERT_TX_FAILED);

			// Clear RECEIVE_DETECT, disable monitoring
			tcpc.reg_recv_detect = 0;
			pd_phy_rx_disable_monitoring();
		} else if (tcpc.tx_type == TCPC_TX_CABLE_RESET) {
			pd_phy_prepare_reset(0);
			pd_tx(0);
			tx_buf_clear(); // No retry
			alert(TCPC_REG_ALERT_TX_SUCCESS);
		} else if (cur_timestamp > tcpc.tx_start_timestamp + USB_PD_RX_TMOUT_US || (tcpc.tx_retry_count == -1)) {
			pd_prepare_message(TCPC_REG_TRANSMIT_TYPE(tcpc.tx_type), TX_LEN(
					((uint16_t)tcpc.tx_payload[0]) | ( ((uint16_t)tcpc.tx_payload[1])<<8 )
											), tcpc.tx_payload);
			pd_tx(0);
			tcpc.tx_start_timestamp = cur_timestamp;

			tcpc.tx_retry_count++;
			if (tcpc.tx_retry_count >= PD_RETRY_COUNT) {
				// Transmission retry failed
				tx_buf_clear();
				alert(TCPC_REG_ALERT_TX_FAILED);
			}
		}
	} else {
		// We are not sending messages, sample the CC lines

		if (tcpc.internal_flags & TCPC_FLAG_LOOKING4CON) {
			if (tcpc.internal_flags & TCPC_FLAG_DRP_TOGGLE_AS_SNK) {
				if (cur_timestamp > tcpc.drp_last_toggle_timestamp + TCPC_TIMER_TOGGLE_SNK) {
					// Timer expired, Switch to SRC (Rp)
					tcpc.internal_flags &=~ TCPC_FLAG_DRP_TOGGLE_AS_SNK;
					tcpc.internal_flags |= TCPC_FLAG_DRP_TOGGLE_AS_SRC;

					pd_cc_set(TCPC_REG_ROLE_CTRL_SET(0, TCPC_REG_ROLE_CTRL_RP(tcpc.cc_role_ctrl), TYPEC_CC_RP, TYPEC_CC_RP));
					tcpc.cc_status[0] = TYPEC_CC_VOLT_OPEN;
					tcpc.cc_status[1] = TYPEC_CC_VOLT_OPEN;
					tcpc.cc_last_sampled_timestamp = cur_timestamp;	// Postponed the next sampling of CC line, debouncing
				}
			} else if (tcpc.internal_flags & TCPC_FLAG_DRP_TOGGLE_AS_SRC) {
				if (cur_timestamp > tcpc.drp_last_toggle_timestamp + TCPC_TIMER_TOGGLE_SRC) {
					// Timer expired, Switch to SRC (Rp)
					tcpc.internal_flags &=~ TCPC_FLAG_DRP_TOGGLE_AS_SRC;
					tcpc.internal_flags |= TCPC_FLAG_DRP_TOGGLE_AS_SNK;

					pd_cc_set(TCPC_REG_ROLE_CTRL_SET(0, 0, TYPEC_CC_RD, TYPEC_CC_RD));
					tcpc.cc_status[0] = TYPEC_CC_VOLT_OPEN;
					tcpc.cc_status[1] = TYPEC_CC_VOLT_OPEN;
					tcpc.cc_last_sampled_timestamp = cur_timestamp;	// Postponed the next sampling of CC line, debouncing
				}
			}
		}

		if (cur_timestamp > tcpc.cc_last_sampled_timestamp + 1000) {
			// Check CC lines every 1mS
			tcpc.cc_last_sampled_timestamp = cur_timestamp;

			// Check CC lines
			tcpc_detect_cc_status(cur_timestamp);

			// Sample the Vbus
			tcpc.vbus_100mv = pd_vbus_read_voltage();

			if (tcpc.vbus_100mv > 40) {
				if (!(tcpc.power_status&TCPC_REG_POWER_STATUS_VBUS_PRES)
						&& (tcpc.power_status_mask&TCPC_REG_POWER_STATUS_VBUS_PRES))
					alert(TCPC_REG_ALERT_POWER_STATUS);
				tcpc.power_status |= TCPC_REG_POWER_STATUS_VBUS_PRES;
			} else {
				if ((tcpc.power_status&TCPC_REG_POWER_STATUS_VBUS_PRES)
						&& (tcpc.power_status_mask&TCPC_REG_POWER_STATUS_VBUS_PRES))
					alert(TCPC_REG_ALERT_POWER_STATUS);
				tcpc.power_status &=~ TCPC_REG_POWER_STATUS_VBUS_PRES;
			}
		}
	}
}
