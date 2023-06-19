class Attribute {
  constructor(ch_uuid, id, val_id, val_type) {
    this.uuid = service_uuid.substr(0, 4) + ch_uuid.substr(0, 4) + service_uuid.substr(8)
    this.ch
    this.id = document.getElementById(id)
    this.val
    this.val_id = document.getElementById(val_id)
    this.val_type = val_type
    this.val_fb
    this.enable_notify = false
    this.enable_write = false
    att_array.push(this)
  }
}

var color = {
  success: "#008800",
  pending: "#AA8800",
  failure: "#AA0000",
}

let att_array = []

const service_uuid = "88a30000-6cfa-440d-8ddd-4a2d48a4812f"
const status = new Attribute("0001", "notifyStatus", "notifyStatus", "string")
const command = new Attribute("0002", null, null, null)
const start_pos = new Attribute("0010", "#position", "#position_start", "int")
const end_pos = new Attribute("0011", "#position", "#position_end", "int")
// const duration = new Attribute("0012", "writeDurationButton", null, "notifyValueDuration", "int")
const speed = new Attribute("0013", "#speed", "#speed_high", "int")
const soft_start = new Attribute("0014", "#speed", "#speed_low", "int")
const intervals = new Attribute("0015", "#interval_num", "#interval_num_val", "int")
const int_delay = new Attribute("0016", "#interval_delay", "#interval_delay_val", "int")

//notify setup
status.enable_notify = true
start_pos.enable_notify = true
end_pos.enable_notify = true
// st duration.enable_notify = true
speed.enable_notify = true
soft_start.enable_notify = true
intervals.enable_notify = true
int_delay.enable_notify = true

//write setup
// command.enable_write = true
start_pos.enable_write = true
end_pos.enable_write = true
// st duration.enable_write = true
speed.enable_write = true
soft_start.enable_write = true
intervals.enable_write = true
int_delay.enable_write = true

async function scanForDevices() {
  try {
    if (!navigator.bluetooth) {
      throw new Error("Web Bluetooth API not available")
    }

    device = await navigator.bluetooth.requestDevice({
      filters: [{ services: [service_uuid] }],
    })
    console.log("Device found:", device.name)
    const listItem = document.createElement("li")
    listItem.textContent = device.name
    document.getElementById("connectButton").disabled = false
  } catch (error) {
    console.error(error)
  }
}

async function connectToDevice() {
  try {
    if (!navigator.bluetooth) {
      throw new Error("Web Bluetooth API not available")
    }
    const server = await device.gatt.connect()
    console.log("Connected to device:", device.name)
    const service = await server.getPrimaryService(service_uuid)
    for (let i = 0; i < att_array.length; i++) {
      att_array[i].ch = await service.getCharacteristic(att_array[i].uuid)
    }
    await readAllValues()
    for (let i = 0; i < att_array.length; i++) {
      if (att_array[i].enable_notify) {
        await att_array[i].ch.startNotifications()
        await att_array[i].ch.addEventListener("characteristicvaluechanged", handleNotification)
      }
    }
  } catch (error) {
    console.error(error)
  }
}

async function handleNotification(event) {
  const value = event.target.value
  const decodedValue = new TextDecoder().decode(value)
  for (let i = 0; i < att_array.length; i++) {
    if (event.target.uuid === att_array[i].uuid) {
      if (att_array[i].val_type === "string") {
        att_array[i].val_id.textContent = await decodedValue
      } else if (att_array[i].val_type === "int") {
        att_array[i].val_fb = parseInt(decodedValue)
        if (att_array[i].val === att_array[i].val_fb) {
          att_array[i].id.style.setProperty("--primary-color", color.success)
        } else {
          att_array[i].id.style.setProperty("--primary-color", color.failure)
          console.log(att_array[i].val, " vs ", att_array[i].val_fb)
        }
      }
    }
  }
}

async function writeValue(att) {
  try {
    const valueToWrite = att.val_id.value
    att.val = parseInt(valueToWrite)
    att.id.style.setProperty("--primary-color", color.pending)
    const valueArray = new TextEncoder().encode(valueToWrite)
    const valueToWriteUint8 = new Uint8Array(valueArray)
    await att.ch.writeValue(valueToWriteUint8)
    console.log("Value written:", valueToWriteUint8)
  } catch (error) {
    console.error(error)
  }
}

async function writeCmd(characteristic, writeValueElement) {
  try {
    const valueToWrite = writeValueElement
    const valueArray = new TextEncoder().encode(valueToWrite)
    const valueToWriteUint8 = new Uint8Array(valueArray)
    await characteristic.writeValue(valueToWriteUint8)
    console.log("Value written:", valueToWriteUint8)
    writeValueElement.value = ""
  } catch (error) {
    console.error(error)
  }
}

async function readValue(att) {
  try {
    const value = await att.ch.readValue()
    const decodedValue = new TextDecoder().decode(value)
    if (att.val_type === "string") {
      att.val_id.textContent = await decodedValue
    } else if (att.val_type === "int") {
      att.val = att.val_id.value = parseInt(decodedValue)
    }
    att.id.style.setProperty("--primary-color", color.success)
  } catch (error) {
    console.error(error)
    att.id.style.setProperty("--primary-color", color.failure)
  }
}

async function readAllValues() {
  for (let i = 0; i < att_array.length; i++) {
    if (att_array[i].enable_notify) {
      await readValue(att_array[i])
    }
  }
}

for (let i = 0; i < att_array.length; i++) {
  if (att_array[i].enable_write) {
    att_array[i].val_id.addEventListener("click", function () {
      writeValue(att_array[i])
    })
  }
}

document.getElementById("scanButton").addEventListener("click", scanForDevices)
document.getElementById("connectButton").addEventListener("click", connectToDevice)
document.getElementById("startButton").addEventListener("click", function () {
  writeCmd(ch_cmd, "start")
})
document.getElementById("stopButton").addEventListener("click", function () {
  writeCmd(ch_cmd, "stop")
})
