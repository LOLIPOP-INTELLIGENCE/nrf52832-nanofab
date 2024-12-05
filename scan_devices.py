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
        # Connect to the device
        async with BleakClient(target_device.address) as client:
            logger.info(f"Connected: {client.is_connected}")
            
            # Get device information
            logger.info("Device services:")
            for service in client.services:
                logger.info(f"Service: {service.uuid}")
                for char in service.characteristics:
                    logger.info(f"  Characteristic: {char.uuid}")
                    logger.info(f"    Properties: {char.properties}")
            
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