#include <Wire.h>
#include "tcpm_driver.h"
#include "FUSB302.h"

const int usb_pd_int_pin = 12;
const int debug_led_pin  = 13;
const int vbus_pin = 4;

#define DFP

// USB-C Specific - TCPM start 1
const struct tcpc_config_t tcpc_config[CONFIG_USB_PD_PORT_COUNT] = {
  {0, fusb302_I2C_SLAVE_ADDR, &fusb302_tcpm_drv},
};
// USB-C Specific - TCPM end 1

enum state {
  STATE_INVALID = -1,
  STATE_DISCONNECTED = 0,
  STATE_CONNECTED,
  STATE_READY,
  STATE_DFP_VBUS_ON,
  STATE_DFP_CONNECTED,
  STATE_DFP_ACCEPT,
  STATE_IDLE,
};

state st = STATE_DISCONNECTED;

#define STATE(x) do { st = STATE_##x; Serial.print("S: " #x "\n"); } while(0)

void vbus_off(void) {
  digitalWrite(vbus_pin, LOW);
#ifdef DFP
  //delay(800);
#endif
  pinMode(vbus_pin, INPUT);
  Serial.print("VBUS OFF\n");
}

void vbus_on(void) {
  Serial.print("VBUS ON\n");
  pinMode(vbus_pin, OUTPUT);
  digitalWrite(vbus_pin, HIGH);
}

void evt_dfpconnect(void) {
  int cc1 = -1, cc2 = -1;
  fusb302_tcpm_get_cc(0, &cc1, &cc2);
  Serial.print("Connected: cc1=");
  Serial.print(cc1);
  Serial.print(" cc2=");
  Serial.print(cc2);
  Serial.print("\n");
  if (cc1 < 2 && cc2 < 2) {
    Serial.print("Nope.\n");
    return;
  }
  fusb302_pd_reset(0);
  fusb302_tcpm_set_msg_header(0, 1, 1); // Source
  if (cc1 > cc2) {
    fusb302_tcpm_set_polarity(0, 0);
    Serial.print("Polarity: CC1 (normal)\n");
  } else {
    fusb302_tcpm_set_polarity(0, 1);
    Serial.print("Polarity: CC2 (flipped)\n");
  }
  fusb302_tcpm_set_rx_enable(0, 1);
  vbus_on();
  STATE(DFP_VBUS_ON);
}

void evt_connect(void) {
  int cc1 = -1, cc2 = -1;
  fusb302_tcpm_get_cc(0, &cc1, &cc2);
  Serial.print("Connected: cc1=");
  Serial.print(cc1);
  Serial.print(" cc2=");
  Serial.print(cc2);
  Serial.print("\n");
  if (cc1 < 2 && cc2 < 2) {
    Serial.print("Nope.\n");
    return;
  }
  fusb302_pd_reset(0);
  fusb302_tcpm_set_msg_header(0, 0, 0); // Sink
  if (cc1 > cc2) {
    fusb302_tcpm_set_polarity(0, 0);
    Serial.print("Polarity: CC1 (normal)\n");
  } else {
    fusb302_tcpm_set_polarity(0, 1);
    Serial.print("Polarity: CC2 (flipped)\n");
  }
  fusb302_tcpm_set_rx_enable(0, 1);
  STATE(CONNECTED);
}

void evt_disconnect(void) {
  vbus_off();
  Serial.print("Disconnected\n");
  fusb302_pd_reset(0);
  fusb302_tcpm_set_rx_enable(0, 0);
#ifdef DFP
  fusb302_tcpm_select_rp_value(0, TYPEC_RP_USB);
  fusb302_tcpm_set_cc(0, TYPEC_CC_RP); // DFP mode
#else
  fusb302_tcpm_set_cc(0, TYPEC_CC_RD); // UFP mode
#endif
  STATE(DISCONNECTED);
}

void send_power_request(uint32_t cap)
{
  int hdr = PD_HEADER(PD_DATA_REQUEST, 0, 0, 0, 1, PD_REV20, 0);
  uint32_t req =
    (1L<<28) | // Object position (fixed 5V)
    (1L<<25) | // USB communications capable
    (0L<<10) | // 0mA operating
    (0L<<0); // 0mA max

  fusb302_tcpm_transmit(0, TCPC_TX_SOP, hdr, &req);
  Serial.print(">REQUEST\n");
}

void send_sink_cap(void)
{
#ifdef DFP
  int hdr = PD_HEADER(PD_DATA_SINK_CAP, 1, 1, 0, 1, PD_REV20, 0);
#else
  int hdr = PD_HEADER(PD_DATA_SINK_CAP, 0, 0, 0, 1, PD_REV20, 0);
#endif
  uint32_t cap =
    (1L<<26) | // USB communications capable
    (0L<<10) | // 0mA operating
    (0L<<0); // 0mA max

  fusb302_tcpm_transmit(0, TCPC_TX_SOP, hdr, &cap);
  Serial.print(">SINK_CAP\n");
#ifdef DFP
  STATE(READY);
#endif
}

int source_cap_timer = 0;

void send_source_cap(void)
{
  int hdr = PD_HEADER(PD_DATA_SOURCE_CAP, 1, 1, 0, 1, PD_REV20, 0);
  uint32_t cap = 0x37019096;

  fusb302_tcpm_transmit(0, TCPC_TX_SOP, hdr, &cap);
  Serial.print(">SOURCE_CAP\n");
  source_cap_timer = 0;
}

void dump_msg(fusb302_rxfifo_tokens sop, int hdr, uint32_t *msg) {
  int len = PD_HEADER_CNT(hdr);
  switch(sop) {
    case fusb302_TKN_SOP:
      Serial.print("RX SOP (");
      break;
    case fusb302_TKN_SOP1:
      Serial.print("RX SOP' (");
      break;
    case fusb302_TKN_SOP2:
      Serial.print("RX SOP\" (");
      break;
    case fusb302_TKN_SOP1DB:
      Serial.print("RX SOP'DEBUG (");
      break;
    case fusb302_TKN_SOP2DB:
      Serial.print("RX SOP\"DEBUG (");
      break;
    default:
      Serial.print("RX ? (");
      break;
  }

  Serial.print(len);
  Serial.print(") [");
  Serial.print(hdr, HEX);
  Serial.print("]");
  for (int i = 0; i < PD_HEADER_CNT(hdr); i++) {
    Serial.print(" ");
    Serial.print(msg[i], HEX);
  }
  Serial.print("\n");
}

void handle_discover_identity(void) {
  int hdr = PD_HEADER(PD_DATA_VENDOR_DEF, 0, 0, 0, 4, PD_REV20, 0);
  uint32_t vdm[4] = {
      0xff008001L |
      (1L<<6), // ACK

      (1L<<30) | // USB Device
      (0L<<27) | // UFP Product Type = Undefined
      (0L<<26) | // No modal operation
      (0L<<23) | // DFP Product Type = Undefined
      0x5acL, // USB VID = Apple

      0L, // XID

      (0x0001L<<16) | // USB PID,
      0x100L // bcdDevice
  };

  fusb302_tcpm_transmit(0, TCPC_TX_SOP, hdr, vdm);
  Serial.print(">VDM DISCOVER_IDENTITY\n");
}

void handle_power_request(uint32_t req) {
  int hdr = PD_HEADER(PD_CTRL_ACCEPT, 1, 1, 0, 0, PD_REV20, 0);

  fusb302_tcpm_transmit(0, TCPC_TX_SOP, hdr, NULL);
  Serial.print(">ACCEPT\n");

  STATE(DFP_ACCEPT);
}

void send_ps_rdy(void) {
  int hdr = PD_HEADER(PD_CTRL_PS_RDY, 1, 1, 0, 0, PD_REV20, 0);
  fusb302_tcpm_transmit(0, TCPC_TX_SOP, hdr, NULL);
  Serial.print(">PS_RDY\n");

  STATE(IDLE);
}

void send_reject() {
  int hdr = PD_HEADER(PD_CTRL_REJECT, 1, 1, 0, 0, PD_REV20, 0);

  fusb302_tcpm_transmit(0, TCPC_TX_SOP, hdr, NULL);
  Serial.print(">REJECT\n");

  STATE(IDLE);
}

void handle_vdm(fusb302_rxfifo_tokens sop, int hdr, uint32_t *msg) {
  switch(*msg) {
    case 0xff008001: // Structured VDM: DISCOVER IDENTITY
      Serial.print("<VDM DISCOVER_IDENTITY\n");
      handle_discover_identity();
      STATE(READY);
      break;
    default:
      Serial.print("<VDM ");
      dump_msg(sop, hdr, msg);
      break;
  }
}

void handle_msg(fusb302_rxfifo_tokens sop, int hdr, uint32_t *msg) {
  int len = PD_HEADER_CNT(hdr);
  int type = PD_HEADER_TYPE(hdr);

  if (len != 0) {
    switch (type) {
      case PD_DATA_SOURCE_CAP:
        Serial.print("<SOURCE_CAP: ");
        Serial.print(msg[0], HEX);
        Serial.print("\n");
        send_power_request(msg[0]);
        break;
      case PD_DATA_REQUEST:
        Serial.print("<REQUEST: ");
        Serial.print(msg[0], HEX);
        Serial.print("\n");
        handle_power_request(msg[0]);
        break;
      case PD_DATA_VENDOR_DEF:
        handle_vdm(sop, hdr, msg);
        break;
      default:
        Serial.print("<UNK DATA ");
        dump_msg(sop, hdr, msg);
        break;
    }
  } else {
    switch (type) {
      case PD_CTRL_ACCEPT:
        Serial.print("<ACCEPT\n");
        break;
      case PD_CTRL_REJECT:
        Serial.print("<REJECT\n");
        break;
      case PD_CTRL_PS_RDY:
        Serial.print("<PS_RDY\n");
        break;
      case PD_CTRL_PR_SWAP:
        Serial.print("<PR_SWAP\n");
        send_reject();
        break;
      case PD_CTRL_DR_SWAP:
        Serial.print("<DR_SWAP\n");
        send_reject();
        break;
      case PD_CTRL_GET_SINK_CAP:
        Serial.print("<GET_SINK_CAP\n");
        send_sink_cap();
        break;
      default:
        Serial.print("<UNK CTL ");
        dump_msg(sop, hdr, msg);
        break;
    }
  }
}

void evt_packet(void) {
  int hdr, len, ret;
  enum fusb302_rxfifo_tokens sop;
  uint32_t msg[16];

  ret = fusb302_tcpm_get_message(0, msg, &hdr, &sop);
  if (ret) {
    // No packet or GoodCRC
    return;
  }

  handle_msg(sop, hdr, msg);
}

void vdm_fun();

void evt_sent(void) {
  switch (st) {
    case STATE_DFP_VBUS_ON:
      STATE(DFP_CONNECTED);
      vdm_fun();
      break;
    case STATE_DFP_ACCEPT:
      send_ps_rdy();
      break;
  }
}

void handle_irq() {
  int irq, irqa, irqb;
  fusb302_get_irq(0, &irq, &irqa, &irqb);
#if 0
  Serial.print("IRQ=");
  Serial.print(irq, HEX);
  Serial.print(" ");
  Serial.print(irqa, HEX);
  Serial.print(" ");
  Serial.print(irqb, HEX);
  Serial.print("\n");
 #endif

  if (irq & TCPC_REG_INTERRUPT_VBUSOK) {
    Serial.print("IRQ: VBUSOK (VBUS=");
    if (fusb302_tcpm_get_vbus_level(0)) {
      Serial.print("ON)\n");
#ifdef DFP
      send_source_cap();
#else
      evt_connect();
#endif
    } else {
      Serial.print("OFF)\n");
      evt_disconnect();
    }
  }
  if (irqa & TCPC_REG_INTERRUPTA_HARDRESET) {
    Serial.print("IRQ: HARDRESET\n");
    evt_disconnect();
  }
  if (irqa & TCPC_REG_INTERRUPTA_TX_SUCCESS) {
    //Serial.print("IRQ: TXSUCCESS\n");
    evt_sent();
  }
  if (irqb & TCPC_REG_INTERRUPTB_GCRCSENT) {
    //Serial.print("IRQ: GCRCSENT\n");
    while(!fusb302_rx_fifo_is_empty(0))
      evt_packet();
  }
}

void vdm_fun() {

  //uint32_t vdm[] = { 0x5ac8010 }; // Get Action List
  //uint32_t vdm[] = { 0x5ac8012, 0x0105, 0x8002<<16 }; // PMU Reset + DFU Hold
  //uint32_t vdm[] = { 0x5ac8011, 0x0809  }; // Get Action List
  //uint32_t vdm[] = { 0x5ac8012, 0x0105, 0x8000<<16 };

  // VDM to mux debug UART over SBU1/2
  uint32_t vdm[] = { 0x5AC8012, 0x01840306};

#ifdef DFP
  int hdr = PD_HEADER(PD_DATA_VENDOR_DEF, 1, 1, 0, sizeof(vdm) / 4, PD_REV20, 0);
  fusb302_tcpm_transmit(0, TCPC_TX_SOP_DEBUG_PRIME_PRIME, hdr, vdm);
#else
  int hdr = PD_HEADER(PD_DATA_VENDOR_DEF, 0, 0, 0, sizeof(vdm) / 4, PD_REV20, 0);
  fusb302_tcpm_transmit(0, TCPC_TX_SOP_DEBUG_PRIME, hdr, vdm);
#endif
  Serial.print(">VDM serial -> SBU1/2\n");

}

int msg_p = 0;
int std_flag = 0;
uint32_t msg_buf[32];

void serial_handler() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n') {
#ifdef DFP
      int hdr = PD_HEADER(PD_DATA_VENDOR_DEF, 1, 1, 0, msg_p + 1, PD_REV20, 0);
#else
      int hdr = PD_HEADER(PD_DATA_VENDOR_DEF, 0, 0, 0, msg_p + 1, PD_REV20, 0);
#endif
      if (std_flag)
        fusb302_tcpm_transmit(0, TCPC_TX_SOP, hdr, msg_buf);
      else
#ifdef DFP
        fusb302_tcpm_transmit(0, TCPC_TX_SOP_DEBUG_PRIME_PRIME, hdr, msg_buf);
#else
        fusb302_tcpm_transmit(0, TCPC_TX_SOP_DEBUG_PRIME, hdr, msg_buf);
#endif
      Serial.print(">VDM");
      if (!std_flag)
        Serial.print("D");
#if 1
      for (int i = 0; i <= msg_p; i++) {
        Serial.print(" ");
        Serial.print(msg_buf[i], HEX);
      }
#endif
      Serial.print("\n");
      msg_p = 0;
      std_flag = 0;
      msg_buf[msg_p] = 0;
    } else if (c == 's') {
      std_flag = 1;
    } else if (c == ',') {
      msg_buf[++msg_p] = 0;
    } else if (c >= '0' && c <= '9') {
      msg_buf[msg_p] <<= 4;
      msg_buf[msg_p] |= c - '0';
    } else if (c >= 'a' && c <= 'f') {
      msg_buf[msg_p] <<= 4;
      msg_buf[msg_p] |= c - 'a' + 10;
    } else if (c >= 'A' && c <= 'F') {
      msg_buf[msg_p] <<= 4;
      msg_buf[msg_p] |= c - 'A' + 10;
    }
  }
}

int cc_debounce = 0;

void state_machine() {
  switch (st) {
    case STATE_DISCONNECTED: {
#ifdef DFP
      int cc1 = -1, cc2 = -1;
      fusb302_tcpm_get_cc(0, &cc1, &cc2);
      Serial.print("Poll: cc1=");
      Serial.print(cc1);
      Serial.print(" cc2=");
      Serial.print(cc2);
      Serial.print("\n");
      delay(100);
      if (cc1 >= 2 || cc2 >= 2)
          evt_dfpconnect();
#endif
      break;
    }
    case STATE_CONNECTED: {
      break;
    }
    case STATE_DFP_VBUS_ON: {
      if (++source_cap_timer > 37) {
        send_source_cap();
      }
      break;
    }
    case STATE_DFP_CONNECTED: {
      break;
    }
    case STATE_DFP_ACCEPT: {
      break;
    }
    case STATE_READY: {
      //vdm_fun();
//#ifndef DFP
      STATE(IDLE);
//#endif
      break;
    }
    case STATE_IDLE: {
      serial_handler();
      break;
    }
    default: {
      Serial.print("Invalid state ");
      Serial.print(st);
      Serial.print("\n");
    }
  }
#ifdef DFP
  if (st != STATE_DISCONNECTED) {
    int cc1 = -1, cc2 = -1;
    fusb302_tcpm_get_cc(0, &cc1, &cc2);
    if (cc1 < 2 && cc2 < 2) {
      if (cc_debounce++ > 5) {
        Serial.print("Disconnect: cc1=");
        Serial.print(cc1);
        Serial.print(" cc2=");
        Serial.print(cc2);
        Serial.print("\n");
        evt_disconnect();
        cc_debounce = 0;
      }
    } else {
      cc_debounce = 0;
    }
  }
#endif
}

void setup() {
  Serial.begin(500000);

  pinMode(usb_pd_int_pin, INPUT);
  digitalWrite(usb_pd_int_pin, HIGH);
  pinMode(debug_led_pin, OUTPUT);
  digitalWrite(debug_led_pin, LOW);
  vbus_off();

  Wire.setClock(400000);
  Wire.begin();

  int reg;
  tcpc_read(0, TCPC_REG_DEVICE_ID, &reg);
  Serial.print("Device ID: 0x");
  Serial.print(reg, HEX);
  Serial.print("\n");
  if (!(reg & 0x80)) {
    Serial.print("Invalid device ID. Is the FUSB302 alive?\n");
    while (1);
  }

  Serial.print("Init\n");
  fusb302_tcpm_init(0);

  fusb302_pd_reset(0);
  fusb302_tcpm_set_rx_enable(0, 0);
  fusb302_tcpm_set_cc(0, TYPEC_CC_OPEN);
  delay(500);

  tcpc_read(0, TCPC_REG_STATUS0, &reg);
  Serial.print("STATUS0: 0x");
  Serial.print(reg, HEX);
  Serial.print("\n");

  handle_irq();

  evt_disconnect();
}

void loop() {
  if (LOW == digitalRead(usb_pd_int_pin)) {
      handle_irq();
  }

  state_machine();
  delay(4);
}
