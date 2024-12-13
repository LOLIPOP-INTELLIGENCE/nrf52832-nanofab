import asyncio
from bleak import BleakScanner, BleakClient
import logging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

TARGET_UUID = "938a803f-f6b3-420b-a95a-10cc7b32b6db"
CONTROL_CHAR_UUID = "a38a803f-f6b3-420b-a95a-10cc7b32b6db"

async def main():
    logger.info("Starting BLE scan...")
    
    devices = await BleakScanner.discover()
    target_device = next((device for device in devices if device.name == "TempSensor"), None)
    
    if not target_device:
        logger.error("Target device not found!")
        return
    
    logger.info(f"Found target device: {target_device.name}")
    
    try:
        async with BleakClient(target_device.address) as client:
            logger.info("Connected!")
            
            while True:
                command = input("\nEnter '1' to start readings, '0' to stop, or 'q' to quit: ")
                
                if command.lower() == 'q':
                    break
                    
                if command in ['0', '1']:
                    await client.write_gatt_char(CONTROL_CHAR_UUID, command.encode())
                    print(f"Sent command: {command}")
                else:
                    print("Invalid command. Use '1' to start, '0' to stop, or 'q' to quit.")

    except Exception as e:
        logger.error(f"Error during operation: {e}")
        logger.exception("Full traceback:")

if __name__ == "__main__":
    asyncio.run(main())