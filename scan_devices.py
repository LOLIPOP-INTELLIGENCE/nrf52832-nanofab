import asyncio
from bleak import BleakScanner, BleakClient
import logging

# Set up logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# The UUID we're looking for (from your C code)
TARGET_UUID = "038a803f-f6b3-420b-a95a-10cc7b32b6db"

async def scan_and_connect():
    logger.info("Starting BLE scan...")
    
    # Scan for devices
    devices = await BleakScanner.discover()
    
    target_device = None
    for device in devices:
        logger.info(f"Found device: {device.name} ({device.address})")
        
        # Check if this is our target device
        if device.metadata.get("uuids") and TARGET_UUID.lower() in [str(uuid).lower() for uuid in device.metadata["uuids"]]:
            target_device = device
            break
    
    if not target_device:
        logger.error("Target device not found!")
        return
    
    logger.info(f"Found target device: {target_device.name} ({target_device.address})")
    
    try:
        async with BleakClient(target_device.address) as client:
            logger.info(f"Connected: {client.is_connected}")
            
            # Define the characteristic UUID
            CHAR_UUID = "a38a803f-f6b3-420b-a95a-10cc7b32b6db"
            
            # Read the string
            value = await client.read_gatt_char(CHAR_UUID)
            string_value = value.decode('utf-8')
            logger.info(f"Received string: {string_value}")
            
            # Keep connection alive
            logger.info("Press Ctrl+C to disconnect...")
            while True:
                await asyncio.sleep(1)
                
    except Exception as e:
        logger.error(f"Error: {str(e)}")

def main():
    try:
        asyncio.run(scan_and_connect())
    except KeyboardInterrupt:
        logger.info("Script terminated by user")
    except Exception as e:
        logger.error(f"Unexpected error: {str(e)}")

if __name__ == "__main__":
    main()