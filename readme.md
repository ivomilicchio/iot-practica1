Entrega práctica 1 de Ivo Milicchio y Lucas Salanitro



Comentarios:

* Fueron realizados TODOS los ejercicios, tanto obligatorios como opcionales
* Las mediciones se hacen generando con valores aleatorios pero están comentadas las instrucciones para hacerlo con el sensor dht11
* Se reemplazó el uso de SPIFFS por LittleFS
* Se utilizó InlfuxDB Cloud como base de datos para las mediciones
* Se utilizó Grafana para consumir las datos de la base de datos de InlfuxDB y mostrarlas en un dashboard
* El historial de mediciones se halla en el endpoint "/history"
* El endpoint "/api/metrics" redirige al dashboard de Grafana con algunas métricas

