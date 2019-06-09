#include <SPI.h>
#include <Ethernet.h>
#include <DFPlayer_Mini_Mp3.h>
#include <SDS011.h>

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield

byte mac[] = { 0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02 };

// 웹페이지 로드를 위해 포트 80으로
EthernetServer server(80);

// Initialize the Ethernet client library
// with the IP address and port of the server 
// that you want to connect to (port 80 is default for HTTP):

String authcode = "0000"; //상호 인증 코드
String deviceID = "resa01";

boolean reading = false;
boolean warn = true;
float p10,p25;
int error;

//단순 인풋 대기인지, 실제 연결 끊김인지 확인을 위해
boolean sensorFail = false;

//인포 호출시 따로 데이터를 주어야 하므로
boolean infoRequest = false;

SoftwareSerial spkSerial(6,7);
SDS011 my_sds;

EthernetClient client;

void setup() {

 // Open serial communications and wait for port to open:
  
    spkSerial.begin (9600);
    mp3_set_serial (spkSerial);      // DFPlayer-mini mp3 module 시리얼 세팅
    delay(1);                     // 볼륨값 적용을 위한 delay
    mp3_set_volume (30);          // 볼륨조절 값 0~30
    
    my_sds.begin(2,3);
    Serial.begin(9600);

    Serial.println("Initialized complete");
    delay (50);
    mp3_play (101);    //부팅음
    delay (2000);
    mp3_play (100);    //부팅음
    delay (3000);
    
  // start the Ethernet connection:

  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    mp3_play (104);    //연결오류
    // no point in carrying on, so do nothing forevermore:
    for(;;)
      ;
  }

  // print your local IP address:
  mp3_play (105); 
  delay(1000);
  
  String IPStr = String(Ethernet.localIP()[0]) + '.' + String(Ethernet.localIP()[1]) + '.' + String(Ethernet.localIP()[2]) + '.' + String(Ethernet.localIP()[3]);
  Serial.print("My IP address: " + IPStr);
  fullread(IPStr);

  Ethernet.begin(mac, Ethernet.localIP());

}

void(* resetFunc) (void) = 0;//declare reset function at address 0

void loop() {
 checkForClient();
}

void checkForClient() {

    EthernetClient client = server.available();
    if (client) {
      Serial.println("new client");
      
      boolean currentLineIsBlank = true;
      boolean sentHeader = false;
      
      while (client.connected()) {
        if (client.available()) {

          String line = client.readStringUntil('\r');
          infoRequest = false;

          if(!sentHeader){
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/html");
            client.println("Connection: close");
            //client.println("Refresh: 5");
            client.println();
            sentHeader = true;
          }

            char c = client.read();
            /*
            if(reading && c == ' ') reading = false;
            if(c == '?') reading = true; //found the ?, begin reading the info
            if(reading){

            String str;
            str = c;
            

            Serial.println("netAction: " + str);
            netAction(str);

            
            }
            */
          
          if (c == '\n' && currentLineIsBlank) {

            Serial.println("insignal: " + line);
            if (line.indexOf("?") > 0) netAction(line);

            sensorFail = false;
            error = my_sds.read(&p25,&p10);
            int failcount = 0;
                
            while(error){
              failcount++;
              //Serial.println("waiting....");
              error = my_sds.read(&p25,&p10);
              delay(100);
                

              //누적실패가 30을 넘으면 이건 실패이므로
              if (failcount > 30){
                sensorFail = true;
                //Serial.println("fuckit. i'm leaving.");
                break;
              }
            }

             //인포호출
             if (infoRequest){
               client.println("<infodata>");
               client.println("<id>" + deviceID + "</id>");
               client.println("<ip>" + String(Ethernet.localIP()[0]) + '.' + String(Ethernet.localIP()[1]) + '.' + String(Ethernet.localIP()[2]) + '.' + String(Ethernet.localIP()[3]) + "</ip>");
               if (sensorFail == false) {
                  client.println("<pm25>" + String(p25) + "</pm25>");
                  client.println("<pm10>" + String(p10) + "</pm10>");
               }
               client.println("</infodata>");
             }
             else
             {
               client.println("<!DOCTYPE HTML>");
               client.println("<title>ResaDevice - " + deviceID + "</title>");
               client.println("<html>");
               client.println("<body>");
               if (sensorFail == false) {
                client.println("P2.5: "+String(p25));
                client.println("P10:  "+String(p10));
                client.println("<br />");
               }
               client.println("</body>");
               client.println("</html>");
             }
            
            

            break;
          }
        if (c == '\n') {
          currentLineIsBlank = true;
        }else if (c != '\r') {
          currentLineIsBlank = false;
        }

        
        //Serial.write(c);

      
      }
    }

    delay(1);

    client.stop();
    Serial.println("client disconnected");
    }

}

/*
 * netAction Request에서는 다음 코멘드가 있음:
 * /auth0000 -> 상호인증코드, 0000 부분이 코드 (없을시 노출안됨)
 * /restart -> 기기 재시작
 * /sayip -> 아이피를 스피커를 통해 출력
 * /status -> 대기정보 + 기기 현재 상태
 */

void netAction(String request)
{
  Serial.println("netAction : OK");
  
  
  //인증 코드 확인
  if (request.indexOf("/auth='" + authcode + "'") > 0) {
    
    Serial.println("Auth : OK");

    //인증코드에서 명령코드를 잘못 인식하는 일이 없도록 제거
    request.replace("/auth='" + authcode + "'" , "");
    Serial.println("Request : " + request);

    //재시작
    if (request.indexOf("/restart") > 0) {
        delay(1);
        client.stop();
        resetFunc();  //call reset
    }

    //정보 호출
    if (request.indexOf("/info") > 0) {
        infoRequest = true;
    }

    //IP주소 출력
    if (request.indexOf("/sayip") > 0) {
        mp3_play (105); 
        delay(1000);
        String IPStr = String(Ethernet.localIP()[0]) + '.' + String(Ethernet.localIP()[1]) + '.' + String(Ethernet.localIP()[2]) + '.' + String(Ethernet.localIP()[3]);
        Serial.print("My IP address: " + IPStr);
        fullread(IPStr);
    }

    //경고 안내 출력
    if (request.indexOf("/warn") > 0) {
        mp3_play (107); 
    }



    
    
  }else{
    client.println("autherror");
  }
}


//전체 리드. 0~9와 '.'만 가능함.
void fullread(String instr)
{
    for (int i = 0; i < instr.length(); i++){
      numread(instr[i]);
      delay(500);
    } 
}

//넘버 리드
void numread(char num)
{
  switch (num)
  {
    case '1':
      mp3_play (1); 
      break;
    
    case '2':
      mp3_play (2); 
      break;
    
    case '3':
      mp3_play (3); 
      break;
    
    case '4':
      mp3_play (4); 
      break;
    
    case '5':
      mp3_play (5); 
      break;
    
    case '6':
      mp3_play (6); 
      break;
    
    case '7':
      mp3_play (7); 
      break;
    
    case '8':
      mp3_play (8); 
      break;
    
    case '9':
      mp3_play (9); 
      break;
    
    case '0':
      mp3_play (10); 
      break;
    
    case '.':
      mp3_play (13); 
      break;
  }
  
}
