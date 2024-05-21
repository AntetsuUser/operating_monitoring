
extern char data_str[20];

// RTCはBCD形式でデータを扱っているので変換しないといけない
byte convertBCD(byte val)
{
    return ((val / 10) << 4) | (val % 10);
}

// それぞれの時間をBCD形式に変換
void ntpToRTCFormat(struct tm* timeInfo, byte RTCFormat[])
{
    RTCFormat[0] = convertBCD(timeInfo->tm_sec);         // 秒
    RTCFormat[1] = convertBCD(timeInfo->tm_min);         // 分
    RTCFormat[2] = convertBCD(timeInfo->tm_hour);        // 時
    RTCFormat[3] = convertBCD(timeInfo->tm_mday);        // 日
    RTCFormat[4] = convertBCD(timeInfo->tm_wday + 1);    // 曜日
    RTCFormat[5] = convertBCD(timeInfo->tm_mon + 1);     // 月
    RTCFormat[6] = convertBCD(timeInfo->tm_year % 100);  // 年
}

// RTCに書き込み
void RTCWrite(struct tm* timeInfo)
{
    byte RTCFormat[7];

    // NTPから取得したデータをRTCに書き込むために変換する
    ntpToRTCFormat(timeInfo, RTCFormat);
    
    Rtc.sync(RTCFormat, sizeof(RTCFormat));
}

// RTCがBCD形式で日付けを持っているのでAsciiに変換する
void RTCDateToStr()
{
    byte x;
    data_str[0] = '2';
    data_str[1] = '0';
    x = Rtc.years();
    data_str[2] = upper2chr(x); // 10の位
    data_str[3] = lower2chr(x); // 1 の位
    
    x = Rtc.months();
    data_str[5] = upper2chr(x);
    data_str[6] = lower2chr(x);
    
    x = Rtc.days();
    data_str[8] = upper2chr(x);
    data_str[9] = lower2chr(x);
    
    x = Rtc.hours();
    data_str[11] = upper2chr(x);
    data_str[12] = lower2chr(x);
    
    x = Rtc.minutes();
    data_str[14] = upper2chr(x);
    data_str[15] = lower2chr(x);
    
    x = Rtc.seconds();
    data_str[17] = upper2chr(x);
    data_str[18] = lower2chr(x);
}

// 上位をAsciiコードに変換して、10の位の数値を抽出
// 右に4ビットシフトし、上位4ビットを取り出す
char upper2chr(byte x) 
{
    return (x >> 4) + 0x30;
}

// 下位をAsciiコードに変換して、1の位の数値を抽出
// 0x0f（下位4ビットを取り出すためのマスク）のビットAND演算を行い、下位4ビットを取り出す
char lower2chr(byte x)
{
    return (x & 0x0f) + 0x30;
}
