#ifndef SCSI_H
#define SCSI_H

// Command operation codes (CDB byte 0).
#define SCSI_CMD_TEST_UNIT_READY 		0x00
#define SCSI_CMD_REQUEST_SENSE 			0x03
#define SCSI_CMD_INQUIRY 				0x12
#define SCSI_CMD_MODE_SENSE_6 			0x1A
#define SCSI_CMD_READ_CAPACITY_10 		0x25
#define SCSI_CMD_READ_10 				0x28
#define SCSI_CMD_WRITE_10 				0x2A

// INQUIRY standard data.
#define SCSI_INQ_PDT_MASK 	0x1F // byte 0: peripheral device type
#define SCSI_INQ_RMB 		0x80 // byte 1: removable medium bit

// MODE SENSE(6).
#define SCSI_MODE_PAGE_RETURN_ALL 	0x3F // page code: all pages
#define SCSI_MODE_HDR_WP 			0x80 // mode header device-specific: write protect

// Fixed-format sense data (REQUEST SENSE / autosense).
#define SCSI_SENSE_CURRENT_FIXED 0x70 // byte 0: response code
#define SCSI_SENSE_ADD_LEN 6          // byte 7: additional sense length

// Sense key (sense byte 2, low nibble).
#define SCSI_SENSE_KEY_MASK 	0x0F
#define SCSI_SK_NOT_READY 		0x02
#define SCSI_SK_MEDIUM_ERROR 	0x03
#define SCSI_SK_HARDWARE_ERROR 	0x04
#define SCSI_SK_UNIT_ATTENTION 	0x06
#define SCSI_SK_DATA_PROTECT 	0x07

#endif
