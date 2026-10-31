// ===== CONJUNTO DE CONFIRMADOS FORA DE ORDEM (INC 7B) =======================

#include "resolvidos.h"
#include "estado.h"

int indiceResolvido(const char* boot, uint32_t seq) {
  for (int i = 0; i < g_resolvidosN; i++) {
    if (g_resolvidos[i].seq == seq && strcmp(g_resolvidos[i].boot, boot) == 0) return i;
  }
  return -1;
}

void removerResolvido(int idx) {
  if (idx < 0 || idx >= g_resolvidosN) return;
  for (int i = idx; i < g_resolvidosN - 1; i++) g_resolvidos[i] = g_resolvidos[i + 1];
  g_resolvidosN--;
}

void marcarResolvido(const char* boot, uint32_t seq) {
  if (indiceResolvido(boot, seq) >= 0) return;
  if (g_resolvidosN >= 32) {
    // Sem espaço: descarta a chave mais antiga. O cursor pode reenviar depois
    // (duplicata), mas não perde o registro.
    removerResolvido(0);
  }
  memcpy(g_resolvidos[g_resolvidosN].boot, boot, 9);
  g_resolvidos[g_resolvidosN].seq = seq;
  g_resolvidosN++;
}
