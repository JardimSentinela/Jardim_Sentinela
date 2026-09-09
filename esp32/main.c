//incluir includes respectivos da biblioteca relacionada aos sensores e ao modulo lora

//exemplos de funcoes das bibliotecas:
double lerSensor(int pino);
void enviarDados(double dados);

// dados e definicoes iniciais que são definidas quando o jardim inicia:
void setup(){
 //variaveis iniciais
 double dados = 0.0;

 //pinos
 int pinoSensor = 4; //exemplo
 int pinoLora = 7; //exemplo

}

// o que o jardim vai ficar executando o tempo que ele estiver ativo:
void loop(){

    //ler sensor e extratir esses dados
    dados = lerSensor(pinoSensor);

    //enviar os dados para a API
    enviarDados(dados);
}

double lerSensor(int pino){
    //implementacao da leitura do sensor
    return 1.0;
}

void enviarDados(double dados){
    //implementacao pra enviar pra API
}