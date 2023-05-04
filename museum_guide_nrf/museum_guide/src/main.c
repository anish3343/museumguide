#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/types.h>
#include <zephyr/kernel.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include <nfc_t2t_lib.h>
#include <nfc/ndef/uri_rec.h>
#include <nfc/ndef/uri_msg.h>

#define GROUP6_FINAL_SERVICE_UUID BT_UUID_128_ENCODE(0x5253FF4B, 0xE47C, 0x4EC8, 0x9792, 0x69FDF492FFF6)

static ssize_t count_read(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset);
static ssize_t url_read(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf, uint16_t len, uint16_t offset);
static ssize_t url_write(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags);
static ssize_t name_write(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf, uint16_t len, uint16_t offset, uint8_t flags);

// Global value that saves state for the characteristic.
uint32_t characteristic_value = 0x7;

uint32_t counter = 0;

char url_buffer[256];
uint8_t url_length = 0;

uint8_t ndef_msg_buf[256];
uint32_t nfc_length = 256;

time_t last_scan_time = 0;

// Set up the advertisement data.
#define DEVICE_NAME "MuseumGuide"
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

static struct bt_data ad[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, GROUP6_FINAL_SERVICE_UUID)
};

// Setup the the service and characteristics.
BT_GATT_SERVICE_DEFINE(lab2_service,
	BT_GATT_PRIMARY_SERVICE(
		BT_UUID_DECLARE_128(GROUP6_FINAL_SERVICE_UUID)
	),
	BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(0x0001), BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
			       BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, url_read, url_write, &characteristic_value),
	BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(0x0002), BT_GATT_CHRC_READ,
			       BT_GATT_PERM_READ, count_read, NULL, &characteristic_value),
	BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(0x0003), BT_GATT_CHRC_WRITE,
			       BT_GATT_PERM_WRITE, NULL, name_write, &characteristic_value),
);


// Callback when a client reads the characteristic.
//
// Documented under name "bt_gatt_attr_read_chrc()"
static ssize_t count_read(struct bt_conn *conn,
			                       const struct bt_gatt_attr *attr,
								   void *buf,
			                       uint16_t len,
								   uint16_t offset)
{
	// Need to encode data into a buffer to send to client.
	uint8_t out_buffer[4] = {0};

	out_buffer[0] = (uint8_t)((counter >> 24) & 0xFF);
	out_buffer[1] = (uint8_t)((counter >> 16) & 0xFF);
	out_buffer[2] = (uint8_t)((counter >> 8) & 0xFF);
	out_buffer[3] = (uint8_t)((counter) & 0xFF);

	// User helper function to encode the output data to send to
	// the client.
	return bt_gatt_attr_read(conn, attr, buf, len, offset, out_buffer, 4);
}

// Callback when a client reads the characteristic.
//
// Documented under name "bt_gatt_attr_read_chrc()"
static ssize_t url_read(struct bt_conn *conn,
			                       const struct bt_gatt_attr *attr,
								   void *buf,
			                       uint16_t len,
								   uint16_t offset)
{
	// User helper function to encode the output data to send to
	// the client.
	return bt_gatt_attr_read(conn, attr, buf, len, offset, url_buffer, url_length);
}

// Callback when a client writes to the characteristic.
//
// Documented under name "bt_gatt_attr_write_func_t()"
static ssize_t url_write(struct bt_conn *conn,
			                       const struct bt_gatt_attr *attr,
								   const void *buf,
			                       uint16_t len,
								   uint16_t offset,
								   uint8_t flags)
{
	memcpy(url_buffer, buf, len);
	url_length = len;

	printk("Received URL \"%s\"\n", url_buffer);

	// The URL has been reset; reset the counter
	counter = 0;

	// Encode NFC message
	static const uint8_t* m_url = (const uint8_t*) url_buffer;

	nfc_length = 256;
	int err;
	err = nfc_ndef_uri_msg_encode( NFC_URI_NONE,
									m_url,
									url_length,
									ndef_msg_buf,
									&nfc_length);

	if(err){
		printk("NFC encoding failed with error %d!\n", err);
	}

	nfc_t2t_emulation_stop();

	/* Set created message as the NFC payload */
	if (nfc_t2t_payload_set(ndef_msg_buf, nfc_length) < 0) {
		printk("Cannot set NFC payload!\n");
	}

	/* Start sensing NFC field */
	if (nfc_t2t_emulation_start() < 0) {
		printk("Cannot start NFC emulation!\n");
	}
	
	return len;
}

// Callback when a client writes to the characteristic.
//
// Documented under name "bt_gatt_attr_write_func_t()"
static ssize_t name_write(struct bt_conn *conn,
			                       const struct bt_gatt_attr *attr,
								   const void *buf,
			                       uint16_t len,
								   uint16_t offset,
								   uint8_t flags)
{
	if(len > 64){
		printk("Name too long (%d characters)! Limit: 64 characters\n", len);
	}
	printk("Name length: %d\n", len);
	char name_buffer[len];
	memcpy(name_buffer, buf, len);

	printk("Received name \"%s\"\n", name_buffer);

	int err;
	err = bt_set_name(name_buffer);
	if(err){
		printk("Name change failed with error %d!\n", err);
	}

	printk("Name changed to \"%s\"\n", name_buffer);

	err = bt_le_adv_stop();
	if(err){
		printk("Advertisement stop failed with error %d!\n", err);
	}

	ad->data = name_buffer;
	ad->data_len = len;
	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	printk("Advertising restarted with new name \"%s\"\n", name_buffer);
}


// Setup callbacks when devices connect and disconnect.
static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		printk("Connection failed (err 0x%02x)\n", err);
	} else {
		printk("Connected\n");
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected (reason 0x%02x)\n", reason);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

static void att_mtu_updated(struct bt_conn *conn, uint16_t tx, uint16_t rx)
{
	printk("MTU exchanged: TX: %d, RX: %d\n", tx, rx);
}

static struct bt_gatt_cb gatt_callbacks = {
	.att_mtu_updated = att_mtu_updated,
};

static void bt_ready(int err)
{
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	printk("Advertising successfully started\n");
}

/* Callback for NFC events */
static void nfc_callback(void * context, nfc_t2t_event_t event, const uint8_t * p_data, size_t data_length)
{
	time_t scan_time = k_uptime_get();
	if(scan_time - last_scan_time < 1000){
		return;
	}
	printk("NFC Read! Total count: %d\n", ++counter);
	last_scan_time = scan_time;
}

void main(void)
{
	int err;

	err = bt_enable(bt_ready);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	bt_gatt_cb_register(&gatt_callbacks);

	printk("BLE configuration done\n");

	uint32_t err_code;
	err_code = nfc_t2t_setup(nfc_callback, NULL);
	if (err_code) {
		printk("NFC init failed (err %d)\n", err_code);
		return;
	}

	printk("NFC configuration done\n");
}
