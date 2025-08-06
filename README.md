# Introducción

Este proyecto comprende la codificación de un Servidor DHCP en el lenguaje de programación C. Contiene tanto el servidor como el cliente que se conecta a él, junto con un Makefile para hacer más ágil la compilación y ejecución de los programas.

## Video explicativo
[Haga click aquí para ir al video](https://youtu.be/HBlyq_6-99g)

# Diagrama UML
![UML DHCP](https://github.com/user-attachments/assets/b3606953-00e5-456b-8d3c-878050b4f0cd)

# Requerimientos
- C con el compilador 'cc', en caso de tener otro compilador como 'gcc' u otros, cambiar el comando en el Makefile.
- Make instalado, para poder correr los comando a través de él, si no quiere o puede usarlo, remítase al Makefile para extraer los comandos que contiene.

# Ejecución

### Cliente-Servidor juntos

Para evitar autenticarse en su usuario administrador con mucha frecuencia, se recomienta el uso del modo super admin antes de ejecutar el programa, con:
```bash
sudo su
```

Si solo desea compilar ambos archivos (server.c y client.c), utilice el comando:
```bash
make
```

Si desea compilar y ejecutar un solo archivo a la vez, utilice:
```bash
make server
```
y
```bash
make client
```
> [!NOTE]
> Recuerde siempre ejecutar primero el servidor, luego cuantas instancias de cliente desee.

### Con Relay agregado

Ejecute el relay en la IP que especifique en el momento de la ejecución, recuerde utilizar la IP de la red a la que está conectado:
```bash
make relay ip=XXX.XXX.XXX.XXX
```

# Aspectos logrados
- DHCP Discover
- DHCP Offer
- DHCP Request
- DHCP ACK
- DHCP Release
- DHCP Broadcast
- Cálculo de submáscara de red
- Servidor separado por hilos
- Lease de IPs
- Asignación de IPs dinámica y delimitada
- DHCP Relay

# Aspectos no logrados
- DHCP NAK
- DHCP Decline

# Conclusiones
A pesar que fue complejo encontrar información al respecto teniendo en cuenta la restricción del lenguaje, la implementación en términos generales fue factible. También hubo diferentes errores con la dirección de memoria, los cuales no tenían solución aparente y cuya correción retrasó los tiempos de desarrollo. La codificación y puesta en funcionamiento del DHCP Relay, se tornó compleja y extensa, puesto que tuvimos que recurrir a otras tecnologías como Docker para hacer que funcione bajo los parámetros solicitados.

# Referencias
- https://www.ibm.com/docs/en/ssw_ibm_i_73/pdf/rzakgpdf.pdf
- https://www.redeszone.net/tutoriales/internet/que-es-protocolo-dhcp/
- https://www.geeksforgeeks.org/dynamic-host-configuration-protocol-dhcp/
- https://www.geeksforgeeks.org/how-to-configure-c-cpp-on-aws-ec2/
- https://www.geeksforgeeks.org/amazon-ec2-creating-an-elastic-cloud-compute-instance/
