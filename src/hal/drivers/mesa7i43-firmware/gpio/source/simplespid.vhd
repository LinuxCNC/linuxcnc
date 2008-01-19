library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.STD_LOGIC_ARITH.ALL;
use IEEE.STD_LOGIC_UNSIGNED.ALL;

--  Uncomment the following lines to use the declarations that are
--  provided for instantiating Xilinx primitive components.
--library UNISIM;
--use UNISIM.VComponents.all;

entity simplespi is
    generic (
      buswidth : integer;
      div : integer;
      bits : integer
      );   
   port ( 
      clk : in std_logic;
      ibus : in std_logic_vector(buswidth-1 downto 0);
      obus : out std_logic_vector(buswidth-1 downto 0);
      loaddata : in std_logic;
      readdata : in std_logic;
      loadcs : in std_logic;
      readcs : in std_logic;
      spiclk : out std_logic;
      spiin : in std_logic;
      spiout: out std_logic;
      spics: out std_logic
       );
end simplespi;

architecture behavioral of simplespi is

-- ssi interface related signals

signal BitCount: integer range 0 to 8 ;
signal RateDiv : integer range 0 to 7;
signal ClockFF: std_logic; 
signal SPISreg: std_logic_vector(buswidth-1 downto 0);
signal Go: std_logic; 
signal Dav: std_logic; 
signal SPIOutDel: std_logic; 
signal CS : std_logic := '1'; 

begin 

   aspiinterface: process (clk, readdata, Go, DAV, SPIOutDel,
                           SPISreg, readcs, CS, ClockFF)
   begin
      if rising_edge(clk) then
         SPIOutDel <= SPISReg(bits-1); -- delay output 1 clock to meet SPI EEPROM hold time
         if loaddata = '1' then 
            SPISreg <= ibus;
            BitCount <= (bits -1);
            Go <= '1';
            Dav <= '0';
            ClockFF <= '0';
            RateDiv <= div;
         end if;
   
         if loadcs = '1' then
            CS <= ibus(0);
         end if;
         
         if Go = '1' then 
            if RateDiv = 0 then
               RateDiv <= div;
               if ClockFF = '0' then
                  ClockFF <= '1';
                  SPISreg <= SPISreg(6 downto 0) & spiin;
               else
                  ClockFF <= '0';
                  BitCount <= BitCount -1;
                  if BitCount = 0 then
                     Go <= '0';
                     Dav <= '1';
                  end if;   
               end if;   
            else               
               RateDiv <= RateDiv -1;
            end if;
         end if;

      end if; -- clk

      obus <= (others => 'Z');
      if readdata =  '1' then
         obus <= SPISReg;
      end if;
      if   readcs =  '1' then
         obus(0) <= CS;
         obus(1) <= Go;
         obus(2) <= Dav;         
      end if;
 
      spiclk <= ClockFF;
      spiout <= SPIOutDel;
      spics <= CS;
   end process aspiinterface;

end Behavioral;
