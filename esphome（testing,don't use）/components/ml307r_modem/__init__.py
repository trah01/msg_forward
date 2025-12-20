import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, text_sensor, sensor, binary_sensor
from esphome.const import (
    CONF_ID,
    CONF_UART_ID,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    STATE_CLASS_MEASUREMENT,
    UNIT_DECIBEL_MILLIWATT,
)

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["text_sensor", "sensor", "binary_sensor"]
MULTI_CONF = False

CONF_SMS_SENDER = "sms_sender"
CONF_SMS_CONTENT = "sms_content"
CONF_SMS_TIMESTAMP = "sms_timestamp"
CONF_SIGNAL_STRENGTH = "signal_strength"
CONF_NETWORK_STATUS = "network_status"
CONF_MODEM_ONLINE = "modem_online"

ml307r_modem_ns = cg.esphome_ns.namespace("ml307r_modem")
ML307RModem = ml307r_modem_ns.class_("ML307RModem", cg.Component, uart.UARTDevice)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ML307RModem),
        cv.Optional(CONF_SMS_SENDER): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_SMS_CONTENT): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_SMS_TIMESTAMP): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_SIGNAL_STRENGTH): sensor.sensor_schema(
            unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_NETWORK_STATUS): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_MODEM_ONLINE): binary_sensor.binary_sensor_schema(),
    }
).extend(uart.UART_DEVICE_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if CONF_SMS_SENDER in config:
        sens = await text_sensor.new_text_sensor(config[CONF_SMS_SENDER])
        cg.add(var.set_sms_sender_sensor(sens))

    if CONF_SMS_CONTENT in config:
        sens = await text_sensor.new_text_sensor(config[CONF_SMS_CONTENT])
        cg.add(var.set_sms_content_sensor(sens))

    if CONF_SMS_TIMESTAMP in config:
        sens = await text_sensor.new_text_sensor(config[CONF_SMS_TIMESTAMP])
        cg.add(var.set_sms_timestamp_sensor(sens))

    if CONF_SIGNAL_STRENGTH in config:
        sens = await sensor.new_sensor(config[CONF_SIGNAL_STRENGTH])
        cg.add(var.set_signal_strength_sensor(sens))

    if CONF_NETWORK_STATUS in config:
        sens = await text_sensor.new_text_sensor(config[CONF_NETWORK_STATUS])
        cg.add(var.set_network_status_sensor(sens))

    if CONF_MODEM_ONLINE in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_MODEM_ONLINE])
        cg.add(var.set_modem_online_sensor(sens))
