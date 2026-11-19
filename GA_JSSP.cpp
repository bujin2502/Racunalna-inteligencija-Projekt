// Genetski algoritam za problem raspoređivanja poslova (Job Shop Scheduling Problem, JSSP).
//
// Kromosom je permutacija s ponavljanjem: svaki posao j pojavljuje se točno
// onoliko puta koliko ima operacija. k-to pojavljivanje posla j označava
// k-tu operaciju tog posla. Dekodiranjem se svaka operacija raspoređuje
// što ranije (nakon prethodne operacije istog posla i nakon zadnje operacije
// na istom stroju). Fitness je makespan (ukupno trajanje rasporeda) - manji je bolji.

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace std;

// Jedna operacija: na kojem se stroju izvodi i koliko traje.
struct Operacija {
    int stroj;
    int trajanje;
};

struct Instanca {
    int broj_poslova = 0;
    int broj_strojeva = 0;
    vector<vector<Operacija>> poslovi;  // poslovi[j][k] = k-ta operacija posla j
};

// Raspoređena operacija u konačnom rasporedu.
struct Termin {
    int stroj;
    int pocetak;
    int kraj;
};

using Kromosom = vector<int>;

struct Parametri {
    string datoteka_instance = "data/yn4.txt";
    string ime_instance;  // prazno = prva instanca u datoteci
    int velicina_populacije = 50;
    int broj_generacija = 500;
    int velicina_turnira = 3;
    double stopa_mutacije = 0.001;
    int interval_resetiranja = 10;
    double stopa_resetiranja = 0.3;
    unsigned seed = random_device{}();
    unsigned broj_dretvi = 0;  // 0 = automatski
    string izlaz_fitness = "best_fitness_per_generation.csv";
    string izlaz_raspored = "best_schedule.csv";
};

// ---------------------------------------------------------------------------
// Učitavanje instance
// ---------------------------------------------------------------------------

// Vraća true ako se redak sastoji od točno dva cijela broja (zaglavlje "n m").
static bool je_zaglavlje(const string& redak, int& n, int& m) {
    istringstream iss(redak);
    string ostatak;
    if (!(iss >> n >> m)) return false;
    return !(iss >> ostatak) && n > 0 && m > 0;
}

// Učitava instancu u OR-Library formatu:
//   n m
//   stroj trajanje stroj trajanje ...   (jedan redak po poslu, m parova)
// Ako je zadano ime, traži se redak "instance <ime>" (kao u data/jobshop1.txt),
// a inače se uzima prvo zaglavlje u datoteci.
Instanca ucitaj_instancu(const string& datoteka, const string& ime) {
    ifstream ulaz(datoteka);
    if (!ulaz) throw runtime_error("Ne mogu otvoriti datoteku: " + datoteka);

    string redak;
    if (!ime.empty()) {
        bool pronadena = false;
        while (getline(ulaz, redak)) {
            istringstream iss(redak);
            string kljucna_rijec, naziv;
            if (iss >> kljucna_rijec >> naziv && kljucna_rijec == "instance" && naziv == ime) {
                pronadena = true;
                break;
            }
        }
        if (!pronadena) throw runtime_error("Instanca '" + ime + "' nije pronađena u " + datoteka);
    }

    Instanca inst;
    bool ima_zaglavlje = false;
    while (getline(ulaz, redak)) {
        if (je_zaglavlje(redak, inst.broj_poslova, inst.broj_strojeva)) {
            ima_zaglavlje = true;
            break;
        }
    }
    if (!ima_zaglavlje) throw runtime_error("Nije pronađeno zaglavlje 'n m' u " + datoteka);

    inst.poslovi.assign(inst.broj_poslova, {});
    for (int j = 0; j < inst.broj_poslova; ++j) {
        for (int k = 0; k < inst.broj_strojeva; ++k) {
            Operacija op{};
            if (!(ulaz >> op.stroj >> op.trajanje))
                throw runtime_error("Nepotpuni podaci za posao " + to_string(j));
            if (op.stroj < 0 || op.stroj >= inst.broj_strojeva || op.trajanje < 0)
                throw runtime_error("Neispravna operacija u poslu " + to_string(j));
            inst.poslovi[j].push_back(op);
        }
    }
    return inst;
}

// ---------------------------------------------------------------------------
// Dekodiranje i fitness
// ---------------------------------------------------------------------------

vector<vector<Termin>> dekodiraj(const Instanca& inst, const Kromosom& kromosom) {
    vector<vector<Termin>> raspored(inst.broj_poslova);
    vector<int> sljedeca_operacija(inst.broj_poslova, 0);
    vector<int> kraj_stroja(inst.broj_strojeva, 0);
    vector<int> kraj_posla(inst.broj_poslova, 0);

    for (int posao : kromosom) {
        const Operacija& op = inst.poslovi[posao][sljedeca_operacija[posao]++];
        int pocetak = max(kraj_stroja[op.stroj], kraj_posla[posao]);
        int kraj = pocetak + op.trajanje;
        raspored[posao].push_back({op.stroj, pocetak, kraj});
        kraj_stroja[op.stroj] = kraj;
        kraj_posla[posao] = kraj;
    }
    return raspored;
}

// Makespan = vrijeme završetka zadnje operacije.
int fitness(const Instanca& inst, const Kromosom& kromosom) {
    vector<int> sljedeca_operacija(inst.broj_poslova, 0);
    vector<int> kraj_stroja(inst.broj_strojeva, 0);
    vector<int> kraj_posla(inst.broj_poslova, 0);
    int makespan = 0;

    for (int posao : kromosom) {
        const Operacija& op = inst.poslovi[posao][sljedeca_operacija[posao]++];
        int kraj = max(kraj_stroja[op.stroj], kraj_posla[posao]) + op.trajanje;
        kraj_stroja[op.stroj] = kraj;
        kraj_posla[posao] = kraj;
        makespan = max(makespan, kraj);
    }
    return makespan;
}

// Paralelno računa fitness za cijelu populaciju.
void evaluiraj(const Instanca& inst, const vector<Kromosom>& populacija, vector<int>& vrijednosti,
               unsigned broj_dretvi) {
    const size_t n = populacija.size();
    vrijednosti.resize(n);
    broj_dretvi = max(1u, min<unsigned>(broj_dretvi, static_cast<unsigned>(n)));

    auto posao_dretve = [&](size_t od, size_t do_) {
        for (size_t i = od; i < do_; ++i) vrijednosti[i] = fitness(inst, populacija[i]);
    };

    if (broj_dretvi == 1) {
        posao_dretve(0, n);
        return;
    }

    vector<thread> dretve;
    size_t komad = (n + broj_dretvi - 1) / broj_dretvi;
    for (size_t od = 0; od < n; od += komad) dretve.emplace_back(posao_dretve, od, min(n, od + komad));
    for (auto& d : dretve) d.join();
}

// ---------------------------------------------------------------------------
// Genetski operatori
// ---------------------------------------------------------------------------

Kromosom slucajni_kromosom(const Instanca& inst, mt19937& gen) {
    Kromosom k;
    k.reserve(inst.broj_poslova * inst.broj_strojeva);
    for (int j = 0; j < inst.broj_poslova; ++j) k.insert(k.end(), inst.broj_strojeva, j);
    shuffle(k.begin(), k.end(), gen);
    return k;
}

// Turnirska selekcija: nasumično se odabire k jedinki iz populacije i vraća se
// indeks najbolje među njima (one s najmanjim makespanom).
size_t turnir(const vector<int>& fitnessi, int k, mt19937& gen) {
    uniform_int_distribution<size_t> dis(0, fitnessi.size() - 1);
    size_t najbolji = dis(gen);
    for (int i = 1; i < k; ++i) {
        size_t kandidat = dis(gen);
        if (fitnessi[kandidat] < fitnessi[najbolji]) najbolji = kandidat;
    }
    return najbolji;
}

// POX križanje (Precedence Operation Crossover).
// Skup poslova se nasumično podijeli na dva dijela. Dijete 1 preuzima poslove
// iz prvog dijela na istim pozicijama kao roditelj 1, a preostale pozicije
// popunjava poslovima iz drugog dijela redoslijedom kojim se javljaju u roditelju 2.
// Dijete 2 nastaje simetrično. Rezultat je uvijek ispravan kromosom.
pair<Kromosom, Kromosom> krizanje_pox(const Kromosom& r1, const Kromosom& r2, int broj_poslova,
                                      mt19937& gen) {
    bernoulli_distribution pola(0.5);
    vector<char> u_skupu(broj_poslova);
    for (auto& b : u_skupu) b = pola(gen);

    auto napravi_dijete = [&](const Kromosom& a, const Kromosom& b) {
        Kromosom dijete(a.size(), -1);
        for (size_t i = 0; i < a.size(); ++i)
            if (u_skupu[a[i]]) dijete[i] = a[i];
        size_t poz = 0;
        for (int posao : b) {
            if (u_skupu[posao]) continue;
            while (dijete[poz] != -1) ++poz;
            dijete[poz] = posao;
        }
        return dijete;
    };

    return {napravi_dijete(r1, r2), napravi_dijete(r2, r1)};
}

// Mutacija zamjenom: svaki gen se s vjerojatnošću stopa_mutacije zamijeni s
// genom na nasumičnoj poziciji. Zamjena čuva ispravnost kromosoma.
void mutiraj(Kromosom& k, double stopa_mutacije, mt19937& gen) {
    uniform_real_distribution<> vjerojatnost(0.0, 1.0);
    uniform_int_distribution<size_t> pozicija(0, k.size() - 1);
    for (size_t i = 0; i < k.size(); ++i)
        if (vjerojatnost(gen) < stopa_mutacije) swap(k[i], k[pozicija(gen)]);
}

// ---------------------------------------------------------------------------
// Glavna petlja
// ---------------------------------------------------------------------------

pair<Kromosom, int> genetski_algoritam(const Instanca& inst, const Parametri& p) {
    mt19937 gen(p.seed);
    const size_t vel = p.velicina_populacije;

    ofstream izlaz_fitness(p.izlaz_fitness);
    ofstream izlaz_raspored(p.izlaz_raspored);
    if (!izlaz_fitness || !izlaz_raspored) throw runtime_error("Ne mogu otvoriti izlazne datoteke");

    vector<Kromosom> populacija;
    populacija.reserve(vel);
    for (size_t i = 0; i < vel; ++i) populacija.push_back(slucajni_kromosom(inst, gen));

    vector<int> fitnessi;
    evaluiraj(inst, populacija, fitnessi, p.broj_dretvi);

    size_t idx = min_element(fitnessi.begin(), fitnessi.end()) - fitnessi.begin();
    Kromosom najbolje_rjesenje = populacija[idx];
    int najbolji_fitness = fitnessi[idx];

    for (int generacija = 1; generacija <= p.broj_generacija; ++generacija) {
        vector<Kromosom> nova;
        nova.reserve(vel);
        nova.push_back(najbolje_rjesenje);  // elitizam: najbolje rješenje uvijek preživljava

        while (nova.size() < vel) {
            const Kromosom& r1 = populacija[turnir(fitnessi, p.velicina_turnira, gen)];
            const Kromosom& r2 = populacija[turnir(fitnessi, p.velicina_turnira, gen)];
            auto [d1, d2] = krizanje_pox(r1, r2, inst.broj_poslova, gen);
            mutiraj(d1, p.stopa_mutacije, gen);
            mutiraj(d2, p.stopa_mutacije, gen);
            nova.push_back(move(d1));
            if (nova.size() < vel) nova.push_back(move(d2));
        }

        // Resetiranje: svakih interval_resetiranja generacija dio populacije
        // (stopa_resetiranja) zamjenjuje se novim slučajnim jedinkama radi
        // održavanja raznolikosti. Elitna jedinka (indeks 0) se ne dira.
        if (p.interval_resetiranja > 0 && generacija % p.interval_resetiranja == 0 && vel > 1) {
            int broj = static_cast<int>(vel * p.stopa_resetiranja);
            uniform_int_distribution<size_t> pozicija(1, vel - 1);
            for (int i = 0; i < broj; ++i) nova[pozicija(gen)] = slucajni_kromosom(inst, gen);
        }

        populacija = move(nova);
        evaluiraj(inst, populacija, fitnessi, p.broj_dretvi);

        idx = min_element(fitnessi.begin(), fitnessi.end()) - fitnessi.begin();
        if (fitnessi[idx] < najbolji_fitness) {
            najbolji_fitness = fitnessi[idx];
            najbolje_rjesenje = populacija[idx];
        }

        cout << "Generacija " << generacija << ": najbolji fitness = " << najbolji_fitness << '\n';
        izlaz_fitness << generacija << ',' << najbolji_fitness << '\n';
    }

    auto raspored = dekodiraj(inst, najbolje_rjesenje);
    izlaz_raspored << "Job ID,Machine ID,Start Time,End Time\n";
    for (size_t j = 0; j < raspored.size(); ++j)
        for (const Termin& t : raspored[j])
            izlaz_raspored << j << ',' << t.stroj << ',' << t.pocetak << ',' << t.kraj << '\n';

    return {najbolje_rjesenje, najbolji_fitness};
}

// ---------------------------------------------------------------------------
// Komandna linija
// ---------------------------------------------------------------------------

void ispisi_pomoc(const char* program) {
    Parametri d;
    cout << "Upotreba: " << program << " [opcije]\n\n"
         << "  -f, --datoteka PUTANJA   datoteka s instancom (zadano: " << d.datoteka_instance << ")\n"
         << "  -i, --instanca IME       ime instance unutar datoteke, npr. ft10 za data/jobshop1.txt\n"
         << "  -p, --populacija N       veličina populacije (zadano: " << d.velicina_populacije << ")\n"
         << "  -g, --generacije N       broj generacija (zadano: " << d.broj_generacija << ")\n"
         << "  -k, --turnir N           veličina turnira (zadano: " << d.velicina_turnira << ")\n"
         << "  -m, --mutacija X         vjerojatnost mutacije po genu (zadano: " << d.stopa_mutacije << ")\n"
         << "      --reset-interval N   svakih N generacija resetira dio populacije, 0 = isključeno (zadano: "
         << d.interval_resetiranja << ")\n"
         << "      --reset-stopa X      udio populacije koji se resetira (zadano: " << d.stopa_resetiranja << ")\n"
         << "  -s, --seed N             seed generatora slučajnih brojeva (zadano: slučajan)\n"
         << "  -t, --dretve N           broj dretvi za evaluaciju, 0 = automatski (zadano: 0)\n"
         << "      --izlaz-fitness F    CSV s najboljim fitnessom po generaciji (zadano: " << d.izlaz_fitness << ")\n"
         << "      --izlaz-raspored F   CSV s najboljim rasporedom (zadano: " << d.izlaz_raspored << ")\n"
         << "  -h, --pomoc              ispiše ovu pomoć\n";
}

Parametri parsiraj_argumente(int argc, char** argv) {
    Parametri p;
    for (int i = 1; i < argc; ++i) {
        string a = argv[i];
        auto vrijednost = [&]() -> string {
            if (i + 1 >= argc) throw runtime_error("Opcija " + a + " zahtijeva vrijednost");
            return argv[++i];
        };
        if (a == "-h" || a == "--pomoc") { ispisi_pomoc(argv[0]); exit(0); }
        else if (a == "-f" || a == "--datoteka") p.datoteka_instance = vrijednost();
        else if (a == "-i" || a == "--instanca") p.ime_instance = vrijednost();
        else if (a == "-p" || a == "--populacija") p.velicina_populacije = stoi(vrijednost());
        else if (a == "-g" || a == "--generacije") p.broj_generacija = stoi(vrijednost());
        else if (a == "-k" || a == "--turnir") p.velicina_turnira = stoi(vrijednost());
        else if (a == "-m" || a == "--mutacija") p.stopa_mutacije = stod(vrijednost());
        else if (a == "--reset-interval") p.interval_resetiranja = stoi(vrijednost());
        else if (a == "--reset-stopa") p.stopa_resetiranja = stod(vrijednost());
        else if (a == "-s" || a == "--seed") p.seed = static_cast<unsigned>(stoul(vrijednost()));
        else if (a == "-t" || a == "--dretve") p.broj_dretvi = static_cast<unsigned>(stoul(vrijednost()));
        else if (a == "--izlaz-fitness") p.izlaz_fitness = vrijednost();
        else if (a == "--izlaz-raspored") p.izlaz_raspored = vrijednost();
        else throw runtime_error("Nepoznata opcija: " + a + " (pokreni s --pomoc)");
    }
    if (p.velicina_populacije < 2) throw runtime_error("Populacija mora imati barem 2 jedinke");
    if (p.broj_generacija < 0) throw runtime_error("Broj generacija ne smije biti negativan");
    if (p.velicina_turnira < 1) throw runtime_error("Veličina turnira mora biti barem 1");
    if (p.broj_dretvi == 0) p.broj_dretvi = max(1u, thread::hardware_concurrency());
    return p;
}

int main(int argc, char** argv) {
    try {
        Parametri p = parsiraj_argumente(argc, argv);
        Instanca inst = ucitaj_instancu(p.datoteka_instance, p.ime_instance);

        cout << "Instanca: " << p.datoteka_instance
             << (p.ime_instance.empty() ? "" : " (" + p.ime_instance + ")") << ", " << inst.broj_poslova
             << " poslova x " << inst.broj_strojeva << " strojeva, seed = " << p.seed << '\n';

        auto [rjesenje, najbolji_fitness] = genetski_algoritam(inst, p);
        cout << "Najbolji fitness (makespan): " << najbolji_fitness << '\n';
    } catch (const exception& e) {
        cerr << "Greška: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
