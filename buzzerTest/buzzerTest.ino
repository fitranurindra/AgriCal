int buzzer = 17; //deklarasi pin buzzer

void setup() {
  // put your setup code here, to run once:
  pinMode(buzzer, OUTPUT); //setup pin buzzer menjadi output
}

void loop() {
  // put your main code here, to run repeatedly:
  tone(buzzer, 1000); //membunyikan buzzer sebesar 200Hz, boleh diubah-ubah sesuai dengan keinginan, semakin tinggi frekuensi semakin tinggi pula suara buzzernya
  delay(2000);
  tone(buzzer, 1250);
  delay(2000);
}