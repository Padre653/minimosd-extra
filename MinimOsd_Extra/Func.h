
extern struct loc_flags lflags;  // все булевые флаги кучей


void NOINLINE millis_plus(uint32_t *dst, uint16_t inc) {
    *dst = millis() + inc;
}


void NOINLINE long_plus(uint32_t *dst, uint16_t inc) {
    *dst +=  inc;
}

int NOINLINE long_diff(uint32_t *l1, uint32_t *l2) {
    return (int)(l1-l2);
}


static inline boolean getBit(byte Reg, byte whichBit) {
    return  Reg & (1 << whichBit);
}

float NOINLINE get_converth(){
    return pgm_read_float(&measure->converth);
}

float NOINLINE get_converts(){
    return pgm_read_float(&measure->converts);
}

float NOINLINE f_div1000(float f){
    return f/1000;
}

void NOINLINE mav_message_start(byte len, byte time){
    mav_msg_ttl=seconds + time;// time to show
    mav_msg_len = len;
    mav_msg_shift = count02s;
    
//    lflags.flgMessage=1;
}

int NOINLINE normalize_angle(int a){
    if(a<0)   a+=360;
    if(a>360) a-=360;
    return a;
}

uint16_t NOINLINE time_since(uint32_t *t){
    return (uint16_t)(millis() - *t); // loop time no more 1000 ms

}

static void inline reset_setup_data(){ // called on any screen change
    memset((byte *)chan_raw_middle, 0, sizeof(chan_raw_middle)); // clear channels middle
}

byte get_switch_time(byte n){
    uint16_t val = sets.autoswitch_times;
    while(true){
       if(n-- == 0) return val & 0xf;
       
       val = val >> 4;
    }
}

void doScreenSwitch(){
	lflags.got_data=1; // redraw even no news
	
	//extern void reset_setup_data();
	reset_setup_data();
	
	union {
	    point p;
	    uint16_t i;
	} upi;
	
	upi.p = readPanel(0); // read flags for new screen
	screen_flags = upi.i;
//	screen_flags = (upi.i & 0xff)<<8 | (upi.i>>8) ;
DBG_PRINTF("screen flags %x\n", screen_flags);

}

#define USE_AUTOSWITCH 1

static void pan_toggle(){
    byte old_panel=panelN;

    uint16_t ch_raw;
//    static uint16_t last_chan_raw=0; in vars.h

    if(sets.ch_toggle <= 2) // disabled
	return;
    else if(sets.ch_toggle == 3) 
	ch_raw = PWM_IN;	// 1 - используем внешний PWM для переключения экранов
    else if(sets.ch_toggle >= 5 && sets.ch_toggle <= 8)
	ch_raw = chan_raw[sets.ch_toggle-1];
    else 
        ch_raw = chan_raw[7]; // в случае мусора - канал 8

    if(ch_raw < 1000 || ch_raw > 3000) return; // not in valid range - no switch

//	автоматическое управление OSD  (если режим не RTL или CIRCLE) смена режима туда-сюда переключает экран
    if (sets.ch_toggle == 4){
      if ((osd_mode != 6) && (osd_mode != 7)){
        if (osd_off_switch != osd_mode){
            osd_off_switch = osd_mode;
            //osd_switch_time = millis();
            millis_plus(&osd_switch_time,0);
            if (osd_off_switch == osd_switch_last){
              lflags.rotatePanel = 1;
            }
        }
//        if ((millis() - osd_switch_time) > 2000){
        if ( time_since(&osd_switch_time) > 2000){
          osd_switch_last = osd_mode;
        }
      }
    }
    else  {
      if (!sets.switch_mode){  //Switch mode by value
        /*
	    Зазор канала = диапазон изменения / (число экранов+1)
	    текущий номер = приращение канала / зазор
        */
        int d = (1900-1100)/sets.n_screens;
        byte n = ch_raw>1100 ?(ch_raw-1100)/d : 0 ;
        //First panel
        if ( panelN != n) {
          panelN = n;
        }
      } else{ 			 //Rotation switch
        byte ch_on=0;
//*
        if(sets.flags.flags.chkSwitch200) {
    	    if(last_chan_raw)
        	ch_on = (ch_raw - last_chan_raw) > 200;
            last_chan_raw = ch_raw;
        } else
//*/
            ch_on = (ch_raw > 1500);

        if(sets.flags.flags.chkSwitchOnce) { // once at 1 -> 0
            if (ch_on) { // in HIGH range
                lflags.last_sw_ch = 1;
            } else { // выключено
                if(lflags.last_sw_ch) lflags.rotatePanel = 1;
                lflags.last_sw_ch = 0;
            }
        } else {
            if (ch_on) { // full on and valid
                if (osd_switch_time < millis()){ // переключаем сразу, а при надолго включенном канале переключаем каждые 0.5 сек
                    lflags.rotatePanel = 1;
                    //osd_switch_time = millis() + 500;
                    millis_plus(&osd_switch_time, 500);
                }
            }
        }// once
      }
    }
    if(lflags.rotatePanel){
next_panel:
	lflags.rotatePanel = 0;
        panelN++;
        if (panelN > sets.n_screens)
            panelN = 0;

    }
#ifdef USE_AUTOSWITCH
        else {
            if(sets.flags.flags.AutoScreenSwitch && autoswitch_time && autoswitch_time<seconds) { // автопереключение активно и время вышло
DBG_PRINTLN("switch by AutoSwitch");
               lflags.autosw=1; // авторежим
               lflags.rotatePanel=1; // переключить
            }
    }
#endif

    if(old_panel != panelN){
DBG_PRINTF("switch from %d to %d\n",old_panel, panelN);

	doScreenSwitch();

#ifdef USE_AUTOSWITCH
       if(sets.flags.flags.AutoScreenSwitch && lflags.autosw && sets.n_screens>0 && panelN == sets.n_screens) 
           goto next_panel;  // при автопереключении пропускаем пустой экран
       
       byte swt = get_switch_time(panelN);
DBG_PRINTF("autoswitch time=%d N=%d\n", swt, panelN);
       if(swt) { // установлено время переключения
           autoswitch_time = seconds + swt; // следующее
//         if(autoswitch_time < seconds) { // overflow    
//         }
       } else {        // no autoswitch
           if(lflags.autosw) goto next_panel;  // при автопереключении пропускаем  экраны без автопереключения
           autoswitch_time=0; // при ручном переключении отменить автомат
       }
       
#endif

	if(!lflags.autosw) {
	    static const char msg[] PROGMEM = "Screen 0";

	    // strcpy_P((char *)mav_message,msg); 10 bytes more
	    const char *cp;
	    byte *wp;
	    for(cp=msg, wp=mav_message;;){
	        byte c=pgm_read_byte(cp++);
	        *wp++ = c;
	        if(c==0) break;
	    }
	
	    mav_message[sizeof(msg)-2] += panelN;
	
	    mav_message_start(sizeof(msg)-1,3); // len, time
	}

       lflags.autosw=0; // once

    }

}



//------------------ Battery Remaining Picture ----------------------------------

static char setBatteryPic(uint16_t bat_level,byte *bp)
{

    if(bat_level>128) {
	*bp++=0x89; // нижняя полная, работаем с верхней
	bat_level -=128;
    }else {
	bp[1] = 0x8d; // верхняя пустая, работаем с нижней
	if(bat_level <= 17 && lflags.blinker){
	    *bp   = 0x20;
	    bp[1] = 0x20;
	    return 1;	// если совсем мало то пробел вместо батареи - мигаем
	}
    }

#if 0
    // разбиваем участок 0..128 на 5 частей
  if(bat_level <= 26){
    *bp   = 0x8d;
  }
  else if(bat_level <= 51){
    *bp   = 0x8c;
  }
  else if(bat_level <= 77){
    *bp   = 0x8b;
  }
  else if(bat_level <= 103){
    *bp   = 0x8a;
  }
  else 
    *bp   = 0x89;
#else
    byte n = bat_level / 26;
    
    *bp   = 0x8d - n;

#endif

    return 0;
}

//------------------ Home Distance and Direction Calculation ----------------------------------

int NOINLINE grad_to_sect(int grad){
    //return round(grad/360.0 * 16.0)+1; //Convert to int 1-16.
    
    return (grad*16 + 180)/360 + 1; //Convert to int 1-16.
}


static float NOINLINE diff_coord(float &c1, float &c2){
    return (c1 - c2) * 111319.5;
}


static float /* NOINLINE */ distance(float x, float y){
    return sqrt(sq(x) + sq(y));
}

static void setHomeVars()
{
    float dstlon, dstlat;


    if(osd_fix_type!=0) lflags.gps_active=1; // если что-то появилось то запомним что было

#ifdef IS_COPTER
 #ifdef IS_PLANE
    // copter & plane - differ by model_type
    if(sets.model_type == 0){  //plane
	if(osd_throttle > 50 && takeoff_heading == -400) // lock runway direction
	    takeoff_heading = osd_heading;
    }

    if (lflags.motor_armed && !lflags.was_armed ){ // on motors arm 
	lflags.osd_got_home = 0;   			//    reset home: Хуже не будет, если ГПС был а при арме исчез то наф такой ГПС
//	if(osd_fix_type>=3) {					//  if GPS fix OK
//	    en=1;                			         // lock home position
//	}  само сделается через пару строк
        lflags.was_armed = lflags.motor_armed; // only once
    } 


 #else // pure copter
  //Check disarm to arm switching.
  if (lflags.motor_armed && !lflags.was_armed){
    lflags.osd_got_home = 0;	//If motors armed, reset home 
//    if(osd_fix_type>=3) {	//  if GPS fix OK
//        en=1;                   // lock home position
//    } само сделается через пару строк

    lflags.was_armed = lflags.motor_armed; // only once
  }


 #endif
#else // not copter
 #ifdef IS_PLANE

    if(osd_throttle > 10 && takeoff_heading == -400)
	takeoff_heading = osd_heading;

    if (lflags.motor_armed && !lflags.last_armed_status){ // plane can be armed too
	lflags.osd_got_home = 0;	//If motors armed, reset home in Arducopter version
	takeoff_heading = osd_heading;
    }

    lflags.last_armed_status = lflags.motor_armed;
 
 #endif
#endif

    if(!lflags.osd_got_home && osd_fix_type >= 3 ) { // first home lock on GPS 3D fix - ну или если фикс пришел уже после арма
        osd_home = osd_pos; // lat, lon & alt

        //osd_alt_cnt = 0;
        lflags.osd_got_home = 1;
    } else if(lflags.osd_got_home){
	{
            float scaleLongDown = cos(abs(osd_home.lat) * 0.0174532925);
            //DST to Home
            dstlat = diff_coord(osd_home.lat, osd_pos.lat);
            dstlon = diff_coord(osd_home.lon, osd_pos.lon) * scaleLongDown;
        }
        //osd_home_distance = sqrt(sq(dstlat) + sq(dstlon));
        osd_home_distance = distance(dstlat, dstlon);
	dst_x=(int)fabs(dstlat); 		// prepare for RADAR
	dst_y=(int)fabs(dstlon);

        { //DIR to Home
            int bearing;

            bearing = atan2(dstlat, -dstlon) * 57.295775; //absolute home direction
        
//	        if(bearing < 0) bearing += 360;//normalization
            bearing = 90 + bearing - 180 - osd_heading;//absolut return direction  //relative home direction
            //while(bearing < 0) bearing += 360;//normalization
            bearing=normalize_angle(bearing);

            osd_home_direction = grad_to_sect(bearing); 
        }
  }
}

void NOINLINE calc_max(float &dst, float src){
    if (dst < src) dst = src;

}



#define USE_FILTER 1 

#if defined(USE_FILTER)
void NOINLINE filter( float &dst, float val, const byte k){ // комплиментарный фильтр 1/k
    //dst = (val * k) + dst * (1.0 - k); 
    //dst = val * k + dst - dst*k;
    //dst = (val-dst)*k + dst;
    dst+=(val-dst)/k;
}

void inline filter( float &dst, float val){ // комплиментарный фильтр 1/10
    filter(dst,val,10);
}
#endif

void NOINLINE float_add(float &dst, float val){
    dst+=val;
}

// вычисление нужных переменных
// накопление статистики и рекордов
void setFdataVars()
{
    float time_1000; 

    
//    uint16_t time_lapse = (uint16_t)(millis() - runtime); // loop time no more 1000 ms
    uint16_t time_lapse = time_since(&runtime); // loop time no more 1000 ms
    //runtime = millis();
    millis_plus(&runtime,0);

    time_1000 = f_div1000(time_lapse); // in seconds


#if defined(USE_FILTER)
                          // voltage in mV, current in 10mA
        filter(power, (osd_vbat_A / (1000 * 100.0) * osd_curr_A )); // комплиментарный фильтр 1/10
#else
	{
            float pow= (osd_vbat_A / (1000 * 100.0) * osd_curr_A );
            power += (pow - power) * 0.1; // комплиментарный фильтр 1/10
        }
        //dst+=(val-dst)*k;
        //vertical_speed += ((osd_climb * get_converth() ) * 60  - vertical_speed) * 0.1; // комплиментарный фильтр 1/10
        //float vs=(osd_climb * get_converth() ) * 60;
        // vertical_speed += (vs  - vertical_speed) * 0.1; // комплиментарный фильтр 1/10
#endif


    //Moved from panel because warnings also need this var and panClimb could be off
#if defined(USE_FILTER)
        filter(vertical_speed, (osd_climb * get_converth() ) *  60); // комплиментарный фильтр 1/10
#else
	{
            float speed_raw= (osd_climb * get_converth() ) *  60;
            vertical_speed += (speed_raw - vertical_speed) * 0.1; // комплиментарный фильтр 1/10
        }
        //dst+=(val-dst)*k;
        //vertical_speed += ((osd_climb * get_converth() ) * 60  - vertical_speed) * 0.1; // комплиментарный фильтр 1/10
        //float vs=(osd_climb * get_converth() ) * 60;
        // vertical_speed += (vs  - vertical_speed) * 0.1; // комплиментарный фильтр 1/10
#endif

        if(max_battery_reading < osd_battery_remaining_A) // мы запомним ее еще полной
            max_battery_reading = osd_battery_remaining_A;

#ifdef IS_PLANE
//                              Altitude above ground in meters, expressed as * 1000 (millimeters)
// osd_home_alt = osd_alt_mav*1000 - mavlink_msg_global_position_int_get_relative_alt(&msg.m);

//    osd_alt_to_home = (osd_alt_mav - osd_home_alt/1000.0); // ->  mavlink_msg_global_position_int_get_relative_alt(&msg.m)/1000;

    if (!lflags.in_air  && (int)osd_alt_to_home > 5 && osd_throttle > 10){
	lflags.in_air = 1; // взлетели!
	trip_distance = 0;
    }
#endif


    //if (osd_groundspeed > 1.0) trip_distance += (osd_groundspeed * time_lapse / 1000.0);
    if(lflags.osd_got_home && lflags.motor_armed) 
        float_add(trip_distance, osd_groundspeed * time_1000);

    float_add(mah_used, (float)osd_curr_A * time_1000 / (3600.0 / 10.0));

    { // isolate RSSI calc
        uint16_t rssi_v = rssi_in;

        if((sets.RSSI_raw % 2 == 0))  {
            uint16_t l=sets.RSSI_16_low, h=sets.RSSI_16_high;
            bool rev=false;

            if(l > h) {
                l=h;
                h=sets.RSSI_16_low;
                rev=true;
            }

            if(rssi_v < l) rssi_v = l;
            if(rssi_v > h) rssi_v = h;

            rssi = (int16_t)(((float)rssi_v - l)/(h-l)*100.0f);
            //rssi = map(rssi_v, l, h, 0, 100); +200 bytes

            if(rssi > 100) rssi = 100;
            if(rev) rssi=100-rssi;
        } else 
            rssi = rssi_v;
    }

  //Set max data
#ifdef IS_COPTER
 #ifdef IS_PLANE
    if((sets.model_type == 0 && lflags.in_air) || lflags.motor_armed){
 #else
    if (lflags.motor_armed)  {
 #endif
#else
 #ifdef IS_PLANE
  if (lflags.in_air){
 #else
    if(0){ // neither copter nor plane
 #endif
#endif

    long_plus(&total_flight_time_milis, time_lapse);

    
    //if (osd_home_distance > max_home_distance) max_home_distance = osd_home_distance;
    float f=osd_home_distance;
    calc_max(max_home_distance, f);
    calc_max(max_osd_airspeed, osd_airspeed);
    calc_max(max_osd_groundspeed, osd_groundspeed);
    calc_max(max_osd_home_alt, osd_alt_mav);
    calc_max(max_osd_windspeed, osd_windspeed);
    calc_max(max_osd_power, power);

    f=osd_curr_A;
    calc_max(max_osd_curr_A, f);
    calc_max(max_osd_climb, osd_climb);
    f=-osd_climb;
    calc_max(min_osd_climb, f);
  }
}



void NOINLINE gps_norm(float &dst, long f){
    dst = f / GPS_MUL;
}


void NOINLINE set_data_got() {
    lastMAVBeat = millis();
    //millis_plus(&lastMAVBeat, 0);
    lastMavSeconds=seconds;

    lflags.got_data = 1;
#ifdef DEBUG
    if(!lflags.input_active){ // first got packet
	max_dly=0;
    }
#endif
    lflags.input_active=1;
}


void NOINLINE delay_telem(){
        delayMicroseconds((1000000/TELEMETRY_SPEED*10)); //время приема 1 байта
}

void NOINLINE delay_byte(){
    if(!Serial.available_S())
        delay_telem();
}



// чтение пакетов нужного протокола
static void getData(){
//LED_BLINK;

    bool got=false;

again:
    if(lflags.input_active || lflags.data_mode || lflags.blinker) {

#if defined(USE_MAVLINK)
	read_mavlink();
#elif defined(USE_UAVTALK)
	extern bool uavtalk_read(void);
	uavtalk_read();
#elif defined(USE_MWII)
	extern bool mwii_read(void);
	mwii_read();
#elif defined(USE_LTM)
	extern void read_ltm(void);
	read_ltm();
#else
#error "No data protocol defined"
#endif

	lflags.data_mode=lflags.input_active; // if not received any then flag clears

    } else {

	memset( &msg.bytes[0], 0, sizeof(msg.bytes)); // clear packet buffer to initial state
    
#if defined(AUTOBAUD)
	Serial.end();
	static uint8_t last_pulse = 15; // 57600 by default
	uint8_t pulse=255;

	{ // isolate PT and SPEED
		uint32_t pt = millis() + 100; // не более 0.1 секунды
	
	        for(byte i=250; i!=0; i--){
	            if(millis()>pt) break; // not too long
	            long t=pulseIn(PD0, 0, 2500); // 2500uS * 250 = 
	            if(t>255) continue;   // too long - not single bit
	            uint8_t tb = t;       // it less than 255 so convert to byte
	            if(tb==0) continue;   // no pulse at all
	            if(tb<pulse) pulse=tb;// find minimal possible - it will be bit time
	        }
	}
	
	if(pulse == 255)    pulse = last_pulse; // no input at all - use last
	else                last_pulse = pulse; // remember last correct time
	
	// F_CPU   / BAUD for 115200 is 138
	// 1000000 / BAUD for 115200 is 8.68uS
	//  so I has no idea about pulse times - thease simply measured

	long speed;
	byte rate;
	if(     pulse < 11) 	{ speed = 115200; rate = 2;  }  // *4 /2
	else if(pulse < 19) 	{ speed =  57600; rate = 4;  }  // *4 /4
	else if(pulse < 29) 	{ speed =  38400; rate = 6;  }  // *4 /6
	else if(pulse < 40) 	{ speed =  28800; rate = 8;  }
	else if(pulse < 60) 	{ speed =  19200; rate = 16; }
	else if(pulse < 150)	{ speed =   9600; rate = 32; }
	else                    { speed =   4800; rate = 64; }

	stream_rate = rate;
#ifdef DEBUG
	OSD::setPanel(3,2);
	osd.printf_P(PSTR("pulse=%d speed=%ld"),pulse, speed);
#endif
	serial_speed=speed; // store detected port speed for show
	Serial.flush();	// clear serial buffer from garbage
	Serial.begin(speed);
#endif    
	
	lflags.data_mode=true; // пробуем почитать данные
	goto again;
    }

}

bool NOINLINE timeToScreen(){ // we should renew screen 
    return lflags.need_redraw && !vsync_wait;
}

#if defined(DEBUG)
inline uint16_t freeRam () {
  extern uint16_t __heap_start, *__brkval; 
  byte v; 
  return (uint16_t) &v - (__brkval == 0 ? (uint16_t) &__heap_start : (uint16_t) __brkval); 
}
#endif

