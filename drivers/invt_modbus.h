/*  invt_modbus.h - Driver for invt UPS hr, ht series connected via modbus ASCII
 *
 *  Copyright (C)
 *    2022 Michael Manerko <splien.ma@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#ifndef NUT_INVT_MODBUS
#define NUT_INVT_MODBUS

/* serial access parameters */
#define BAUD_RATE 9600
#define PARITY 'N'
#define DATA_BIT 8
#define STOP_BIT 1

/* modbus access parameters */
#define MODBUS_SLAVE_ID 1


#endif /* NUT_INVT_MODBUS */

/* REGISTER MAP

UPS 
INVT HR11 series
INVT HT11 series
SNR-UPS-ONRT (3000-20000) series
HIDEN EXPERT UDC9200S-RT 1-3 кВА
HIDEN EXPERT UDC9200H-RT 6-10 кВА

RS232 MODBUS ASCII

Telemetry (Function code: 0x03)
No. Name                        Data type(Hi-Lo)    Coefficient Unit Remark
0   Bypass voltage Phase A      Unsigned int        0.1         V   Used for compatibility
1   Bypass voltage Phase A      Unsigned int        0.1         V   HT11 series just have phase A, the registers of phase B and C be reserved.
2   Bypass voltage Phase B      Unsigned int        0.1         V        
3   Bypass voltage Phase C      Unsigned int        0.1         V        
4   Bypass current Phase A      Unsigned int        0.1         A        
5   Bypass current Phase B      Unsigned int        0.1         A        
6   Bypass current Phase C      Unsigned int        0.1         A
7   Bypass frequency Phase A    Unsigned int        0.01        Hz        
8   Bypass frequency Phase B    Unsigned int        0.01        Hz        
9   Bypass frequency Phase C    Unsigned int        0.01        Hz        
10  Bypass PF_A                 Unsigned int        0.01                
11  Bypass PF_B                 Unsigned int        0.01                
12  Bypass PF_C                 Unsigned int        0.01                
13  Input voltage Phase A       Unsigned int        0.1         V   HT11 series just have phase A, the registers of phase B and C be reserved.
14  Input voltage Phase B       Unsigned int        0.1         V        
15  Input voltage Phase C       Unsigned int        0.1         V        
16  Input current Phase A       Unsigned int        0.1         A        
17  Input current Phase B       Unsigned int        0.1         A        
18  Input current Phase C       Unsigned int        0.1         A        
19  Input frequency Phase A     Unsigned int        0.01        Hz        
20  Input frequency Phase B     Unsigned int        0.01        Hz        
21  Input frequency Phase C     Unsigned int        0.01        Hz        
22  Input PF_A                  Unsigned int        0.01                
23  Input PF_B                  Unsigned int        0.01                
24  Input PF_C                  Unsigned int        0.01                
25  Output voltage Phase A      Unsigned int        0.1         V   HT11 series just have phase A, the registers of phase B and C be reserved.
26  Output voltage Phase B      Unsigned int        0.1         V        
27  Output voltage Phase C      Unsigned int        0.1         V        
28  Output current Phase A      Unsigned int        0.1         A        
29  Output current Phase B      Unsigned int        0.1         A        
30  Output current Phase C      Unsigned int        0.1         A        
31  Output frequency Phase A    Unsigned int        0.01        Hz        
32  Output frequency Phase B    Unsigned int        0.01        Hz        
33  Output frequency Phase C    Unsigned int        0.01        Hz        
34  Output PF_A                 Unsigned int        0.01        
35  Output PF_B                 Unsigned int        0.01                
36  Output PF_C                 Unsigned int        0.01                
37  Output kVA Phase A          Unsigned int        0.1/1       kVA/VA  HT11 series just have phase A, the registers of phase B and C be reserved. The coefficient of 1-3K is 1，unit is VA, other series coefficient is 0.1，unit is kVA
38  Output kVA Phase B          Unsigned int        0.1         kVA        
39  Output kVA Phase C          Unsigned int        0.1         kVA        
40  Output kW Phase A           Unsigned int        0.1/1       kW/W        
41  Output kW Phase B           Unsigned int        0.1         kW        
42  Output kW Phase C           Unsigned int        0.1         kW        
43  Output kVar Phase A         Unsigned int        0.1/1       kVar/Var        
44  Output kVar Phase B         Unsigned int        0.1         kVar        
45  Output kVar Phase C         Unsigned int        0.1         kVar        
46  Load percent Phase A        Unsigned int        0.1         %        
47  Load percent Phase B        Unsigned int        0.1         %        
48  Load percent Phase C        Unsigned int        0.1         %        
49  Environment temperature     Unsigned int        0.1         ℃        
50  Battery voltage positive    Unsigned int        0.1         V    HT11 series just have positive, the registers of negative be reserved. Battery current: Charge>0,Discharge<0
51  Battery voltage negative    Unsigned int        0.1         V        
52  Battery current positive    int                 0.1         A        
53  Battery current negative    int                 0.1         A        
54  Battery temperature         Unsigned int        0.1         ℃        
55  Battery remain time         Unsigned int        0.1         min        
56  Battery capacity            Unsigned int        0.1         %        
57  Bypass fan running time     Unsigned int        1           H        
58 - 67   Reserved                                
68  Monitor series number       Unsigned int        1                
69  First monitor version number  Unsigned int      1                
70  Second monitor version number Unsigned int      1                
78  UPS series number           Unsigned int        1                   Bit0-Bit5: UPS series
                                                                        7:HT11(6-20 KVA)
                                                                        8:HT11(1-3 KVA)
                                                                        Bit6-Bit15:Reserved
79  UPS Type                    Unsigned int        　                  Bit0-Bit2：Input and output phase
                                                                        1: 3in-3out
                                                                        2: 3in -1out
                                                                        3: 1in -1out
                                                                        4: 1in -3out
                                                                        5: 2in -2out
                                                                        Bit3-Bit15:Reserved

Telesignalization (Function code: 0x04)
No. Name                    Data type(Hi-Lo)    Remark
81  Load On Source            Unsigned int      0：None
                                                1：UPS Supply
                                                2：Bypass Supply
82  Battery Status            Unsigned int      0：Not Work
                                                1：Float Charge
                                                2：Boost Charge
                                                3：Discharge
83  Battery Connect Status    Unsigned int      0：Not Connect
                                                1：Connect
84  Maintain Cb Status        Unsigned int      0：Open
                                                1：Close 
85  EPO                       Unsigned int      0：None
                                                1：EPO
86  Invertor Ready Capacity   Unsigned int      0：Enough
                                                1：Not Enough
87  Generator Input           Unsigned int      0：Disconnect
                                                1：Connected
88  Input Fail                Unsigned int      0：Normal
                                                1：Abnormal
89  Bypass Sequence Fail      Unsigned int      0：Normal
                                                1：Abnormal
90  Bypass Voltage Fail       Unsigned int      0：Normal
                                                1：Abnormal
91  Bypass Fail               Unsigned int      0：Normal
                                                1：Abnormal
92  Bypass Over Load          Unsigned int      0：No
                                                1：Yes
93  Bypass Over Load Timeout  Unsigned int      0：No
                                                1：Yes
94  Bypass Untrack            Unsigned int      0：No
                                                1：Yes
95  Tx Time Limit             Unsigned int      0：No
                                                1：Yes
96  Output Shorted            Unsigned int      0：No
                                                1：Yes
97  Battery EOD               Unsigned int      0：No
                                                1：Yes
98  Battery Test Begin (Reserved) Unsigned int  0：No
                                                1：Yes
99  Battery Test Result       Unsigned int      0: No Test
                                                1:Test Success
                                                2:Test Fail
                                                3:Testing
100 Battery Manual Test (Reserved) Unsigned int 0：No
                                                1：Yes
101 Battery Maintain Result   Unsigned int      0:No Maintain
                                                1:Maintain success
                                                2: Maintain Fail
                                                3:Maintaining
102 Stop Test (Reserved)      Unsigned int  
103 Fault Clear (Reserved)    Unsigned int  
104 Hislog Clear (Reserved)   Unsigned int  
105 On Ups Inhibited          Unsigned int      0: Invertor On Enable
                                                1: Invertor On Disable
106 Manual Tx Bypass          Unsigned int      0: No
                                                1: Yes
107 Battery Volt Low          Unsigned int      0: No
                                                1: Yes
108 Battery Reverse           Unsigned int      0: No
                                                1: Yes
109 REC Status                Unsigned int      0:OFF
                                                1:Soft Start
                                                2:Normal Work
110 Input Neutral Lost        Unsigned int      0: No Lost
                                                1: Lost
111 Bypass Fan Fail           Unsigned int      0: Normal
                                                1: Fail
112 Lost N+X Redundant        Unsigned int      0: No Lost
                                                1: Lost
113 EOD System Inhibited      Unsigned int      0: No
                                                1: Inhibited
114 CT Weld Reverse           Unsigned int      0: Normal
                                                1: Reverse
115 Electrolyte Leakage       Unsigned int      0: Normal
                                                1: Leakage
116 Sensor status             Unsigned int      Corresponding bit is 1 indicate the sensor is disconnected, 0 means it is connected.
                                                Bit0: Battery temperature sensor
                                                Bit1: Environment temperature sensor
                                                Bit2-Bit15：Reserve
117 Reserved    
118 Integrated Alarm          Unsigned int      Bit0：1 Alarm, 0 Normal
                                                Bit1：1 Fault, 0 Normal
                                                Bit2-Bit15：Reserve
121 Unit 1 Pull               Unsigned int      0: Pull Out   1: Join In
122 Unit 1 REC Fail           Unsigned int      0: Normal   1: Abnormal
123 Unit 1 INV Fail           Unsigned int      0: Normal   1: Abnormal
124 Unit 1 REC Over Temperature Unsigned int    0: Normal   1: Abnormal
125 Unit 1 Fan Fail           Unsigned int      0: Normal     1: Abnormal
126 Unit 1 INV Over Load      Unsigned int      0: Normal   1: Abnormal
127 Unit 1 INV Over Load Timeout  Unsigned int  0: Normal   1: Abnormal
128 Unit 1 INV Over Temperature Unsigned int    0: Normal   1: Abnormal
129 Unit 1 INV Protect        Unsigned int      0: Normal   1: Abnormal
130 Unit 1 Manual Shutdown    Unsigned int      0: Normal   1: Shutdown
131 Reserved    
132 Reserved    
133 Unit 2 Pull               Unsigned int  0: Pull Out   1: Join In
134 Unit 2 REC Fail           Unsigned int  0: Normal   1: Abnormal
135 Unit 2 INV Fail           Unsigned int  0: Normal   1: Abnormal
136 Unit 2 REC Over Temperature Unsigned int  0: Normal   1: Abnormal
137 Unit 2 Fan Fail           Unsigned int  0: Normal   1: Abnormal
138 Unit 2 INV Over Load      Unsigned int  0: Normal   1: Abnormal
139 Unit 2 INV Over Load Timeout  Unsigned int  0: Normal   1: Abnormal
140 Unit 2 INV Over Temperature Unsigned int  0: Normal   1: Abnormal
141 Unit 2 INV Protect        Unsigned int  0: Normal   1: Abnormal
142 Unit 2 Manual Shutdown    Unsigned int  0: Normal   1: Shutdown
143 Reserved    
144 Reserved    
145 Unit 3 Pull               Unsigned int  0: Pull Out   1: Join In
146 Unit 3 REC Fail           Unsigned int  0: Normal   1: Abnormal
147 Unit 3 INV Fail           Unsigned int  0: Normal   1: Abnormal
148 Unit 3 REC Over Temperature Unsigned int  0: Normal   1: Abnormal
149 Unit 3 Fan Fail           Unsigned int  0: Normal   1: Abnormal
150 Unit 3 INV Over Load      Unsigned int  0: Normal   1: Abnormal
151 Unit 3 INV Over Load Timeout  Unsigned int  0: Normal   1: Abnormal
152 Unit 3 INV Over Temperature Unsigned int  0: Normal   1: Abnormal
153 Unit 3 INV Protect        Unsigned int  0: Normal   1: Abnormal
154 Unit 3 Manual Shutdown    Unsigned int  0: Normal   1: Shutdown
155 Reserved    
156 Reserved    
157 Unit 4 Pull               Unsigned int  0: Pull Out   1: Join In
158 Unit 4 REC Fail           Unsigned int  0: Normal   1: Abnormal
159 Unit 4 INV Fail           Unsigned int  0: Normal   1: Abnormal
160 Unit 4 REC Over Temperature Unsigned int  0: Normal   1: Abnormal
161 Unit 4 Fan Fail           Unsigned int  0: Normal   1: Abnormal
162 Unit 4 INV Over Load      Unsigned int  0: Normal   1: Abnormal
163 Unit 4 INV Over Load Timeout  Unsigned int  0: Normal   1: Abnormal
164 Unit 4 INV Over Temperature Unsigned int  0: Normal   1: Abnormal
165 Unit 4 INV Protect        Unsigned int  0: Normal   1: Abnormal
166 Unit 4 Manual Shutdown    Unsigned int  0: Normal   1: Shutdown
167 Reserved    
168 Reserved    
169 Unit 5 Pull               Unsigned int  0: Pull Out   1: Join In
170 Unit 5 REC Fail           Unsigned int  0: Normal   1: Abnormal
171 Unit 5 INV Fail           Unsigned int  0: Normal   1: Abnormal
172 Unit 5 REC Over Temperature Unsigned int  0: Normal   1: Abnormal
173 Unit 5 Fan Fail           Unsigned int  0: Normal   1: Abnormal
174 Unit 5 INV Over Load      Unsigned int  0: Normal   1: Abnormal
175 Unit 5 INV Over Load Timeout  Unsigned int  0: Normal   1: Abnormal
176 Unit 5 INV Over Temperature Unsigned int  0: Normal   1: Abnormal
177 Unit 5 INV Protect        Unsigned int  0: Normal   1: Abnormal
178 Unit 5 Manual Shutdown    Unsigned int  0: Normal   1: Shutdown
179 Reserved    
180 Reserved    
181 Unit 6 Pull               Unsigned int  0: Pull Out   1: Join In
182 Unit 6 REC Fail           Unsigned int  0: Normal   1: Abnormal
183 Unit 6 INV Fail           Unsigned int  0: Normal   1: Abnormal
184 Unit 6 REC Over Temperature Unsigned int  0: Normal   1: Abnormal
185 Unit 6 Fan Fail Unsigned int  0: Normal   1: Abnormal
186 Unit 6 INV Over Load      Unsigned int  0: Normal   1: Abnormal
187 Unit 6 INV Over Load Timeout  Unsigned int  0: Normal   1: Abnormal
188 Unit 6 INV Over Temperature Unsigned int  0: Normal   1: Abnormal
189 Unit 6 INV Protect        Unsigned int  0: Normal   1: Abnormal
190 Unit 6 Manual Shutdown    Unsigned int  0: Normal   1: Shutdown
191 Reserved    
192 Reserved    
193 Unit 7 Pull               Unsigned int  0: Pull Out   1: Join In
194 Unit 7 REC Fail           Unsigned int  0: Normal   1: Abnormal
195 Unit 7 INV Fail           Unsigned int  0: Normal   1: Abnormal
196 Unit 7 REC Over Temperature Unsigned int  0: Normal   1: Abnormal
197 Unit 7 Fan Fail           Unsigned int  0: Normal   1: Abnormal
198 Unit 7 INV Over Load      Unsigned int  0: Normal   1: Abnormal
199 Unit 7 INV Over Load Timeout  Unsigned int  0: Normal   1: Abnormal
200 Unit 7 INV Over Temperature Unsigned int  0: Normal   1: Abnormal
201 Unit 7 INV Protect        Unsigned int  0: Normal   1: Abnormal
202 Unit 7 Manual Shutdown    Unsigned int  0: Normal   1: Shutdown
203 Reserved    
204 Reserved    
205 Unit 8 Pull               Unsigned int  0: Pull Out   1: Join In
206 Unit 8 REC Fail           Unsigned int  0: Normal   1: Abnormal
207 Unit 8 INV Fail           Unsigned int  0: Normal   1: Abnormal
208 Unit 8 REC Over Temperature Unsigned int  0: Normal   1: Abnormal
209 Unit 8 Fan Fail           Unsigned int  0: Normal   1: Abnormal
210 Unit 8 INV Over Load      Unsigned int  0: Normal   1: Abnormal
211 Unit 8 INV Over Load Timeout  Unsigned int  0: Normal   1: Abnormal
212 Unit 8 INV Over Temperature Unsigned int  0: Normal   1: Abnormal
213 Unit 8 INV Protect        Unsigned int  0: Normal   1: Abnormal
214 Unit 8 Manual Shutdown    Unsigned int  0: Normal   1: Shutdown
215 Reserved    
216 Reserved    
217 Unit 9 Pull               Unsigned int  0: Pull Out   1: Join In
218 Unit 9 REC Fail           Unsigned int  0: Normal   1: Abnormal
219 Unit 9 INV Fail           Unsigned int  0: Normal   1: Abnormal
220 Unit 9 REC Over Temperature Unsigned int  0: Normal   1: Abnormal
221 Unit 9 Fan Fail           Unsigned int  0: Normal   1: Abnormal
222 Unit 9 INV Over Load      Unsigned int  0: Normal   1: Abnormal
223 Unit 9 INV Over Load Timeout  Unsigned int  0: Normal   1: Abnormal
224 Unit 9 INV Over Temperature Unsigned int  0: Normal   1: Abnormal
225 Unit 9 INV Protect        Unsigned int  0: Normal   1: Abnormal
226 Unit 9 Manual Shutdown    Unsigned int  0: Normal   1: Shutdown
227 Reserved    
228 Reserved    
229 Unit 10 Pull              Unsigned int  0: Pull Out   1: Join In
230 Unit 10 REC Fail          Unsigned int  0: Normal   1: Abnormal
231 Unit 10 INV Fail          Unsigned int  0: Normal   1: Abnormal
232 Unit 10 REC Over Temperature  Unsigned int  0: Normal   1: Abnormal
233 Unit 10 Fan Fail          Unsigned int  0: Normal   1: Abnormal
234 Unit 10 INV Over Load     Unsigned int  0: Normal   1: Abnormal
235 Unit 10 INV Over Load Timeout Unsigned int  0: Normal   1: Abnormal
236 Unit 10 INV Over Temperature  Unsigned int  0: Normal   1: Abnormal
237 Unit 10 INV Protect       Unsigned int  0: Normal   1: Abnormal
238 Unit 10 Manual Shutdown   Unsigned int  0: Normal   1: Shutdown
239 Reserved    
240 Reserved    

*/
