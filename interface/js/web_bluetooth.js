class Attribute {
  constructor(ch_uuid, id, val_id, val_type, val, en_notify, en_write) {
    this.uuid = service_uuid.substr(0, 4) + ch_uuid.substr(0, 4) + service_uuid.substr(8)
    this.ch
    this.id = document.getElementById(id)
    this.val = val
    this.val_id = document.getElementById(val_id)
    this.val_type = val_type
    this.val_fb
    this.enable_notify = en_notify
    this.enable_write = en_write
    // this.css_val = null
    // this.css_text = null
    att_array.push(this)
  }
}

const color = {
  success: "#008800",
  pending: "#AA8800",
  failure: "#AA0000",
}

const att_array = []

const service_uuid = "88a30000-6cfa-440d-8ddd-4a2d48a4812f"
const status = new Attribute("0001", "notifyStatus", "notifyStatus", "string", null, true, false)
const start_pos = new Attribute("0010", "#position", "#position_start", "int", null, true, true)
const end_pos = new Attribute("0011", "#position", "#position_end", "int", null, true, true)
// const duration = new Attribute("0012", "writeDurationButton", null, "notifyValueDuration", "int")
const speed = new Attribute("0013", "#speed", "#speed_high", "int", null, true, true)
const soft_start = new Attribute("0014", "#speed", "#speed_low", "int", null, true, true)
const intervals = new Attribute("0015", "#interval_num", "#interval_num_val", "int", null, true, true)
const int_delay = new Attribute("0016", "#interval_delay", "#interval_delay_val", "int", null, true, true)

//commands
const cmd_start = new Attribute("0002", null, "#cmd_start", "string", "start", false, true)
const cmd_stop = new Attribute("0002", null, "#cmd_stop", "string", "stop", false, true)
// const cmd_check_conn = new Attribute("0001", null, "#connection", "string", "conn_status", false, )

// //notify setup
// status.enable_notify = true
// start_pos.enable_notify = true
// end_pos.enable_notify = true
// // st duration.enable_notify = true
// speed.enable_notify = true
// soft_start.enable_notify = true
// intervals.enable_notify = true
// int_delay.enable_notify = true
//
// //write setup
// command.enable_write = true
// start_pos.enable_write = true
// end_pos.enable_write = true
// // st duration.enable_write = true
// speed.enable_write = true
// soft_start.enable_write = true
// intervals.enable_write = true
// int_delay.enable_write = true
//
start_pos.css_val = "--value-a"
end_pos.css_val = "--value-b"
speed.css_val = "--value-b"
soft_start.css_val = "--value-a"
intervals.css_val = "--value"
int_delay.css_val = "--value"

start_pos.css_text = "--text-value-a"
end_pos.css_text = "--text-value-b"
speed.css_text = "--text-value-b"
soft_start.css_text = "--text-value-a"
intervals.css_text = "--text-value"
int_delay.css_text = "--text-value"

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
    await enableNotify()
    await enableWrite()
    document.getElementById("#connection").innerHTML = "connected"
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
        att_array[i].val_id.textContent = decodedValue
      } else if (att_array[i].val_type === "int") {
        att_array[i].val_fb = parseInt(decodedValue)
        updateCssVal(att_array[i])
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
    console.log("write", att)
    att.val = parseInt(valueToWrite)
    if (att.id != null) {
      att.id.style.setProperty("--primary-color", color.pending)
    }
    const valueArray = new TextEncoder().encode(valueToWrite)
    const valueToWriteUint8 = new Uint8Array(valueArray)
    await att.ch.writeValue(valueToWriteUint8)
    // console.log("Value written:", valueToWriteUint8)
  } catch (error) {
    console.log(att)
    console.error(error)
  }
}

async function readValue(att) {
  try {
    const value = await att.ch.readValue()
    const decodedValue = new TextDecoder().decode(value)
    if (att.val_type === "string") {
      att.val_id.textContent = decodedValue
      // att.val_id.textContent = await decodedValue
    } else if (att.val_type === "int") {
      att.val = att.val_id.value = parseInt(decodedValue)
      updateCssVal(att)
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

async function enableNotify() {
  for (let i = 0; i < att_array.length; i++) {
    if (att_array[i].enable_notify) {
      await att_array[i].ch.startNotifications()
      await att_array[i].ch.addEventListener("characteristicvaluechanged", handleNotification)
    }
  }
}

async function enableWrite() {
  for (let i = 0; i < att_array.length; i++) {
    if (att_array[i].enable_write) {
      att_array[i].val_id.addEventListener("click", function() {
        writeValue(att_array[i])
      })
      att_array[i].val_id.addEventListener("touchend", function() {
        writeValue(att_array[i])
      })
      att_array[i].val_id.disabled = false
      // console.log("write val enabled", att_array[i].id)
    }
  }
}
document.getElementById("scanButton").addEventListener("click", scanForDevices)
document.getElementById("connectButton").addEventListener("click", connectToDevice)
document.getElementById("connectButton").addEventListener("click", connectToDevice)
// document.getElementById("startButton").addEventListener("click", function() {
//   command.val = "start"
//   writeValue(command)
//   // writeCmd(ch_cmd, "start")
// })
// document.getElementById("stopButton").addEventListener("click", function() {
//   command.val = "stop"
//   writeValue(command)
//   // writeCmd(ch_cmd, "stop")
// })

function updateCssVal(att) {
  if (att.css_val != undefined) {
    att.id.style.setProperty(att.css_val, att.val)
  }
  if (att.css_text != undefined) {
    att.id.style.setProperty(att.css_text, att.val)
  }
}

var check_connection = setInterval(async function() {
  try {
    await status.ch.readValue()
    document.getElementById("#connection").innerHTML = "connected"
  } catch (error) {
    document.getElementById("#connection").innerHTML = "disconnected"
  }
}, 5000)
