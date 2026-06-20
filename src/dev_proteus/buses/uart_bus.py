""" UART bus implementation """

from typing import Dict, Optional
from dev_proteus.buses.bus_base import BusBase, DevType
from dev_proteus.core.protocol import UartTermios


# termios constants (Linux/x86_64)
class const:
    # c_cflag
    CS5 = 0x00000000
    CS6 = 0x00000010
    CS7 = 0x00000020
    CS8 = 0x00000030
    CSTOPB = 0x00000040
    CREAD = 0x00000080
    PARENB = 0x00000100
    PARODD = 0x00000200
    HUPCL = 0x00000400
    CLOCAL = 0x00000800
    CRTSCTS = 0x80000000
    
    # c_iflag
    IGNBRK = 0x00000001
    BRKINT = 0x00000002
    IGNPAR = 0x00000004
    PARMRK = 0x00000008
    INPCK = 0x00000010
    ISTRIP = 0x00000020
    INLCR = 0x00000040
    IGNCR = 0x00000080
    ICRNL = 0x00000100
    IXON = 0x00000200
    IXOFF = 0x00000400
    IXANY = 0x00000800
    IUTF8 = 0x00004000
    
    # c_oflag
    OPOST = 0x00000001
    ONLCR = 0x00000004
    
    # c_lflag
    ISIG = 0x00000001
    ICANON = 0x00000002
    ECHO = 0x00000008
    ECHOE = 0x00000010
    ECHOK = 0x00000020
    ECHONL = 0x00000040
    IEXTEN = 0x00000100
    ECHOCTL = 0x00000400
    ECHOKE = 0x00000800
    
    # c_cc
    VEOF = 0
    VEOL = 1
    VEOL2 = 2
    VERASE = 3
    VINTR = 4
    VKILL = 5
    VMIN = 6
    VQUIT = 7
    VSTART = 8
    VSTOP = 9
    VSUSP = 10
    VTIME = 11
    VREPRINT = 12
    VWERASE = 13
    VLNEXT = 14
    VDISCARD = 15
    
    # baudrate
    B50 = 0x00000001
    B75 = 0x00000002
    B110 = 0x00000003
    B134 = 0x00000004
    B150 = 0x00000005
    B200 = 0x00000006
    B300 = 0x00000007
    B600 = 0x00000008
    B1200 = 0x00000009
    B1800 = 0x0000000A
    B2400 = 0x0000000B
    B4800 = 0x0000000C
    B9600 = 0x0000000D
    B19200 = 0x0000000E
    B38400 = 0x0000000F
    B57600 = 0x00001001
    B115200 = 0x00001002
    B230400 = 0x00001003
    B460800 = 0x00001004
    B921600 = 0x00001005


BAUDRATE_TO_TERMIOS = {
        50:     const.B50,      # 0x00000001
        75:     const.B75,      # 0x00000002
        110:    const.B110,     # 0x00000003
        134:    const.B134,     # 0x00000004
        150:    const.B150,     # 0x00000005
        200:    const.B200,     # 0x00000006
        300:    const.B300,     # 0x00000007
        600:    const.B600,     # 0x00000008
        1200:   const.B1200,    # 0x00000009
        1800:   const.B1800,    # 0x0000000A
        2400:   const.B2400,    # 0x0000000B
        4800:   const.B4800,    # 0x0000000C
        9600:   const.B9600,    # 0x0000000D
        19200:  const.B19200,   # 0x0000000E
        38400:  const.B38400,   # 0x0000000F
        57600:  const.B57600,   # 0x00001001
        115200: const.B115200,  # 0x00001002
        230400: const.B230400,  # 0x00001003
        460800: const.B460800,  # 0x00001004
        921600: const.B921600,  # 0x00001005
    }


class UARTBus(BusBase):
    """ UART bus emulation """

    def __init__(self, name: str, config: Optional[Dict] = None):
        super().__init__(name=name)
        
        self.type = DevType.UART
        self.id = UARTBus.calc_fnv1a(name)
        self.config = config

        self._termios = self._init_termios_from_config(config)
        self._modem = 0

    def _init_termios_from_config(self, config: Dict) -> UartTermios:        
        c_iflag = 0
        c_oflag = 0
        c_cflag = 0
        c_lflag = 0
        
        # --- c_cflag ---
        
        # data_bits
        data_bits = config.get('data_bits', 8)
        if data_bits == 5:
            c_cflag |= const.CS5
        elif data_bits == 6:
            c_cflag |= const.CS6
        elif data_bits == 7:
            c_cflag |= const.CS7
        else:  # 8 by default
            c_cflag |= const.CS8
        
        # stop_bits
        if config.get('stop_bits', 1) == 2:
            c_cflag |= const.CSTOPB
        
        # Четность (parity)
        parity = config.get('parity', 'none')
        if parity == 'even':
            c_cflag |= const.PARENB
        elif parity == 'odd':
            c_cflag |= const.PARENB | const.PARODD
        # 'none' - nothing to add
        
        # flow_control
        flow_control = config.get('flow_control', 'none')
        if flow_control == 'hardware':
            c_cflag |= const.CRTSCTS
        # 'software' is handled in c_iflag        
        
        c_cflag |= const.CREAD              # turn on receiver
        c_cflag |= const.HUPCL              # release DTR when close
        # c_cflag |= const.CLOCAL          # don't turn on - use control lines
        
        # --- c_iflag ---
        
        # flow_control
        if flow_control == 'software':
            c_iflag |= const.IXON | const.IXOFF | const.IXANY
        else:
            # regular input settings
            c_iflag |= const.ICRNL          # convert CR to NL
        
        # Other standart flags for input
        # c_iflag |= const.IGNCR            
        # c_iflag |= const.INLCR            
        # c_iflag |= const.IGNBRK           
        # c_iflag |= const.BRKINT           
        # c_iflag |= const.INPCK            
        # c_iflag |= const.ISTRIP           
        # c_iflag |= const.IUTF8            
        
        # --- c_oflag ---
        
        c_oflag |= const.OPOST              # Turn on post handling output
        c_oflag |= const.ONLCR              # Convert NL to CR-NL
        
        # --- c_lflag ---
        
        # Emulation - not RAW mode
        c_lflag |= const.ISIG
        c_lflag |= const.ICANON
        c_lflag |= const.IEXTEN
        c_lflag |= const.ECHO
        c_lflag |= const.ECHOE
        c_lflag |= const.ECHOK
        c_lflag |= const.ECHOCTL
        c_lflag |= const.ECHOKE
        
        # --- c_line ---
        c_line = 0

        # --- c_cc ---
        #
        NCCS = 32         
        c_cc = bytearray(NCCS)
        c_cc[const.VEOF] = 0x04            # ^D
        c_cc[const.VEOL] = 0x00            # Not defined
        c_cc[const.VERASE] = 0x7F          # ^?
        c_cc[const.VINTR] = 0x03           # ^C
        c_cc[const.VKILL] = 0x15           # ^U
        c_cc[const.VMIN] = 1               # Minimal count of symbols
        c_cc[const.VQUIT] = 0x1C           # ^\ 
        c_cc[const.VSTART] = 0x11          # ^Q
        c_cc[const.VSTOP] = 0x13           # ^S
        c_cc[const.VSUSP] = 0x1A           # ^Z
        c_cc[const.VTIME] = 0              # Timeout
        c_cc[const.VREPRINT] = 0x12        # ^R
        c_cc[const.VWERASE] = 0x17         # ^W
        c_cc[const.VLNEXT] = 0x16          # ^V
        c_cc[const.VDISCARD] = 0x0F        # ^O
        
        # --- c_ispeed / c_ospeed ---
        
        bitrate = config.get('bitrate', 115200)
        termios_speed = BAUDRATE_TO_TERMIOS.get(bitrate, const.B115200)
        
        return UartTermios(
            c_iflag=c_iflag,
            c_oflag=c_oflag,
            c_cflag=c_cflag,
            c_lflag=c_lflag,
            c_line=c_line,
            c_cc=bytes(c_cc),
            c_ispeed=termios_speed,
            c_ospeed=termios_speed
        )
    
    @property
    def termios(self) -> UartTermios:        
        return self._termios
    
    @termios.setter
    def termios(self, term: UartTermios) -> None:
        self._termios = term
    
    @property
    def modem(self) -> int:
        return self._modem
    
    @modem.setter
    def modem(self, modem_arg: int) -> None:
        self._modem = modem_arg

    @staticmethod
    def calc_fnv1a(name: str) -> int:        
        FNV_OFFSET_BASIS = 0x811c9dc5
        FNV_PRIME = 0x01000193

        hash_val = FNV_OFFSET_BASIS
        for c in name.encode('utf-8'):
            hash_val ^= c
            hash_val = (hash_val * FNV_PRIME) & 0xFFFFFFFF

        return hash_val    
    
    def get_available_bytes(self) -> int:
        available_bytes = 0
        
        for _, device in self.devices.items():
            available_bytes += device.get_available_bytes()
        
        return available_bytes
    
    def transaction(self, context: Dict) -> bytes:
        """ Execute UART transaction on all available devices """

        total_result = bytearray()

        data_to_write = context.get("data_to_write")
        size_to_read = context.get("size_to_read")
        
        for _, device in self.devices.items():        
            if data_to_write is not None:
                device.write(data_to_write)
            
            if size_to_read is not None:
                total_result.extend(device.read(size_to_read))             
        
        return bytes(total_result)
