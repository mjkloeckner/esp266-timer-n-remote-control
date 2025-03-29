var received_data;
var main_checkbox, timer_checkbox;
var time_div, system_local_domain, system_ip_addr, wifi_ssid, wifi_rssi;
var system_time, system_time_elem, last_ntp_sync, last_ntp_sync_elem;
var to_time, from_time, timer_save_button;

const time_formatter = new Intl.DateTimeFormat('en-GB', {
    timeZone: 'America/Argentina/Buenos_Aires',
    year: 'numeric',
    month: '2-digit',
    day: '2-digit',
    hour: '2-digit',
    minute: '2-digit',
    hour12: false
});

const query_types = {
    "MAIN_OUTPUT_STATUS": 0,
    "MAIN_OUTPUT_TOGGLE": 1,
    "TIMER_TOGGLE":       2,
    "TIMER_SET_VALUES":   3
}

function rssi_to_percentage(rssi) {
    var quality = 0; 

    if (rssi <= -100) { 
        quality = 0; 
    } else if (rssi >= -50) { 
        quality = 100; 
    } else { 
        quality = 2 * (rssi + 100); 
    } 
    return quality; 
}

function update_checkbox() {
    main_checkbox.setAttribute("checked", received_data["main-output-enabled"]);
    timer_checkbox.setAttribute("checked", received_data["timer-enabled"]);
}

function update_time() {
    system_time = received_data["system-time"];
    system_time_elem.innerHTML = time_formatter.format(new Date(system_time*1000))
        .replaceAll("/", "-")
        .replaceAll(",", "") + " GMT-3";
}

function update_timer() {
    console.log(received_data);
    from_time.value = received_data["from-time"];
    to_time.value = received_data["to-time"];
}

function update_all() {
    update_checkbox();
    update_time();
    update_timer();

    system_local_domain.href = "http://" + received_data["system-local-domain"] + ".local";
    system_local_domain.innerHTML = "http://" + received_data["system-local-domain"] + ".local";

    system_ip_addr.href = received_data["system-ip-addr"];
    system_ip_addr.innerHTML = received_data["system-ip-addr"];

    last_ntp_sync = received_data["last-ntp-sync"];
    last_ntp_sync_elem.innerHTML = time_formatter.format(new Date(last_ntp_sync*1000))
        .replaceAll("/", "-")
        .replaceAll(",", "") + " GMT-3";

    wifi_ssid.innerHTML = received_data["wifi-ssid"];
    const rssi = received_data["wifi-rssi"];
    wifi_rssi.innerHTML = rssi + "dBm (" + rssi_to_percentage(rssi) + "%)";
}

function socket_onmessage_handler(event) {
    received_data = JSON.parse(event.data);

    switch(received_data["type"]) {
        case "all":
            update_all();
            break;
        case "time":
            update_time();
            break;
        case "cb":
            update_checkbox();
            break;
        case "timer":
            update_timer();
            break;
        default:
            console.log("[SOCKET] received_data: type not known, updating all...");
            update_all();
    }
}

function socket_onopen_handler(event) {
    console.log("[SOCKET] Connection established")
    socket.send(query_types["MAIN_OUTPUT_STATUS"]);
}

function handle_click(cb) {
    if(cb == 'main-toggle') {
        socket.send(query_types["MAIN_OUTPUT_TOGGLE"]);
    } else if(cb == 'timer-toggle') {
        socket.send(query_types["TIMER_TOGGLE"]);
    } else {
        console.log("handle_click: wrong target");
    }
}

function update_system_time() {
    socket.send(query_types["MAIN_OUTPUT_STATUS"]);
}

function timer_set_time() {
    var query_json = new Object();

    query_json.from = new Object();
    query_json.from.hour = from_time.value.slice(0, 2);
    query_json.from.minute = from_time.value.slice(3, 5);

    query_json.to = new Object();
    query_json.to.hour = to_time.value.slice(0, 2);
    query_json.to.minute = to_time.value.slice(3, 5);
    console.log(query_json);

    socket.send(query_types["TIMER_SET_VALUES"] + JSON.stringify(query_json));
}

function query_data() {
    main_checkbox = document.getElementById("main-checkbox");
    timer_checkbox = document.getElementById("timer-checkbox");
    system_local_domain = document.getElementById("system-local-domain");
    last_ntp_sync_elem = document.getElementById("last-ntp-sync");
    system_time_elem = document.getElementById("system-time");
    system_ip_addr = document.getElementById("system-ip-addr");
    wifi_ssid = document.getElementById("wifi-ssid");
    wifi_rssi = document.getElementById("wifi-rssi");

    from_time = document.getElementById("from-time");
    to_time = document.getElementById("to-time");

    timer_save_button = document.getElementById("timer-save");
    timer_save_button.addEventListener( "click", timer_set_time, false);

    // setInterval(update_system_time, 30*1000); // update time every 30 secs
}

const socket = new WebSocket("ws:/" + "/" + location.host + ":81");
socket.onmessage = socket_onmessage_handler;
socket.onopen = socket_onopen_handler;
window.onload = query_data;
