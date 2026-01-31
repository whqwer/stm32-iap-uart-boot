/**
 * @file protocol.h
 * @brief IAP Protocol Header - Frame-based firmware update protocol
 * 
 * Protocol Frame Structure:
 * [0x7E] [Length(4B,LSB)] [Version] [Receiver] [Sender] [Data...] [CRC32(4B,LSB)] [0x7E]
 * 
 * Features:
 * - Escape encoding for 0x7E and 0x7A bytes
 * - CRC32 checksum for data integrity
 * - State machine based frame parser
 * - Support for firmware data streaming
 */

#ifndef INC_PROTOCOL_H_
#define INC_PROTOCOL_H_
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// ==================== IAP Command Definitions ====================
// These commands are embedded in the protocol data field
#define CMD_IAP_START      0x01  // Start firmware update (includes firmware size)
#define CMD_IAP_DATA       0x02  // Firmware data packet (includes packet number + data)
#define CMD_IAP_END        0x03  // End firmware update (includes CRC32 checksum)

// ==================== Response Codes ====================
#define RESP_OK            0x00  // Operation successful
#define RESP_ERROR         0x01  // Operation failed

// ==================== CRC32 Functions ====================
/**
 * @brief Calculate CRC32 checksum using polynomial 0x04C11DB7 (standard CRC32)
 * @param data Pointer to data buffer
 * @param len Length of data in bytes
 * @return CRC32 checksum value
 */
uint32_t crc32_c(const uint8_t *data, uint32_t len);

// ==================== Protocol Reception Functions ====================
/**
 * @brief Process received UART data and parse protocol frames
 * @details This function implements a state machine to parse incoming frames,
 *          handle escape sequences, verify CRC, and extract firmware data.
 *          Call this function whenever UART data is received.
 * @param buf Pointer to raw UART received data buffer
 * @param len Length of received data in bytes
 * @return 0 on success, -1 on failure (buffer overflow, invalid frame, etc.)
 */
int32_t Protocol_Receive(uint8_t *buf, uint32_t len);

// ==================== IAP Update Interface ====================
/**
 * @brief Initialize IAP protocol layer
 * @details Must be called once before starting firmware update.
 *          Resets protocol state, clears UART errors, and prepares for reception.
 * @param None
 * @return None
 */
void Protocol_IAP_Init(void);

/**
 * @brief Start IAP firmware update process (blocking)
 * @details Waits for firmware data transmission to complete.
 *          Automatically handles Flash erase and programming.
 * @param timeout_ms Timeout in milliseconds (0 = no timeout)
 * @return 0 on success, -1 on failure, -2 on timeout
 * @note This function blocks until update completes or timeout occurs
 */
int8_t Protocol_IAP_Update(uint32_t timeout_ms);

/**
 * @brief Get current firmware update progress
 * @return Number of bytes received and written to Flash so far
 */
uint32_t Protocol_IAP_GetProgress(void);

/**
 * @brief Get current page index from received frame
 * @return Current page index (from frame_buf[5] and frame_buf[6], LSB format)
 */
uint16_t Protocol_IAP_GetCurrentPageIndex(void);

/**
 * @brief Send a protocol frame over UART
 * @param receiver Receiver ID (destination address)
 * @param sender Sender ID (source address)
 * @param data Pointer to payload data
 * @param data_len Length of payload data in bytes
 * @return None
 */
void send_protocol_frame(uint8_t receiver, uint8_t sender, const uint8_t *data, uint32_t data_len);
#endif /* INC_PROTOCOL_H_ */


