import bleak
import asyncio
import matplotlib.pyplot as plt

SERVICE_UUID = '5253ff4b-e47c-4ec8-9792-69fdf492fff6'
URL_UUID = bleak.normalize_uuid_str('0001')
COUNT_UUID = bleak.normalize_uuid_str('0002')
NAME_UUID = bleak.normalize_uuid_str('0003')


async def get_counts():
    devices = await bleak.BleakScanner.discover(return_adv=True)
    names = []
    counts = []
    for d, a in devices.values():
        if a.service_uuids and SERVICE_UUID in a.service_uuids:
            async with bleak.BleakClient(d.address) as client:
                count = int.from_bytes(await client.read_gatt_char(COUNT_UUID), byteorder='little')
                name = d.name
                names.append(name)
                counts.append(count)
    return (names, counts)


async def list_device_names():
    device_names = []
    devices = await bleak.BleakScanner.discover(return_adv=True)
    for d, a in devices.values():
        if a.service_uuids and SERVICE_UUID in a.service_uuids:
            device_names.append(d.name)
    return device_names


async def modify_device_name(old_name, new_name):
    devices = await bleak.BleakScanner.discover(return_adv=True)
    for d, a in devices.values():
        if a.service_uuids and (SERVICE_UUID in a.service_uuids) and (d.name == old_name):
            async with bleak.BleakClient(d.address) as client:
                try:
                    await asyncio.wait_for(client.write_gatt_char(NAME_UUID, new_name.encode('utf-8')), timeout=15.0)
                except TimeoutError:
                    print('Operation timed out, run list_device_names to check if name was changed')
                print('Device name successfully changed to', new_name)
                return True
    return False


async def read_device_url(device_name):
    devices = await bleak.BleakScanner.discover(return_adv=True)
    for d, a in devices.values():
        if a.service_uuids and (SERVICE_UUID in a.service_uuids) and d.name == device_name:
            async with bleak.BleakClient(d.address) as client:
                url_bytes = bytearray()
                try:
                    url_bytes = await asyncio.wait_for(client.read_gatt_char(URL_UUID), timeout=15.0)
                except TimeoutError:
                    print('Operation timed out, please try again')
                url = url_bytes.decode('utf-8')
                if url == '':
                    url = "No URL set"
                return url
    return False

async def modify_device_url(device_name, url):
    devices = await bleak.BleakScanner.discover(return_adv=True)
    for d, a in devices.values():
        if a.service_uuids and (SERVICE_UUID in a.service_uuids) and d.name == device_name:
            async with bleak.BleakClient(d.address) as client:
                try:
                    await asyncio.wait_for(client.write_gatt_char(URL_UUID, url.encode('utf-8')), timeout=15.0)
                except TimeoutError:
                    print('Operation timed out, try again to see if URL was changed')
                print('URL successfully changed to', url)
                return True
    return False


if __name__ == '__main__':
    while True:
        inp = input("Choose function (help to list functions): ")
        if inp == 'help':
            print("Available Commands:\n\
                plot_counts\n\
                list_device_names\n\
                modify_device_name\n\
                modify_device_url\n\
                quit")
        elif inp == 'plot_counts':
            print("Plotting counts...")
            loop = asyncio.get_event_loop()
            data = loop.run_until_complete(get_counts())
            plt.bar(*data)
            plt.xlabel("Device name")
            plt.ylabel("Counts")
            plt.show()
        elif inp == 'list_device_names':
            print("Listing device names...")
            loop = asyncio.get_event_loop()
            for d in loop.run_until_complete(list_device_names()):
                print(d)
        elif inp == 'modify_device_name':
            old_name = input("Old device name: ")
            new_name = input("New device name: ")
            print("Renaming device...")
            loop = asyncio.get_event_loop()
            if (not loop.run_until_complete(modify_device_name(old_name, new_name))):
                print("No device found with name", old_name)
        elif inp == 'modify_device_url':
            device_name = input("Device name: ")
            loop = asyncio.get_event_loop()
            old_url = loop.run_until_complete(read_device_url(device_name))
            if (not old_url):
                print("No device found with name", device_name)
                continue
            print("Current URL:", old_url)
            url = input("URL: ")
            print("Updating URL...")
            loop = asyncio.get_event_loop()
            if (not loop.run_until_complete(modify_device_url(device_name, url))):
                print("No device found with name", device_name)
        elif inp == 'quit':
            break
        else:
            print('Function not recognized')