#include <sys/types.h>
#include <linux/ip.h>
#include <linux/icmp.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

#define PAQ_SIZE 52
#define LOCALHOST "10.0.2.15"

typedef struct		s_time
{
	long int	ms;
	long int	fms;
}			t_time;

typedef struct		s_glob
{
	__u8		f_ttl;
	int		mttl;
	int		timeout;
	int		queries;
	int		paq_len;
}			t_glob;

t_glob			g_glob = {1, 30, 1, 3, PAQ_SIZE};

void	help(char *s)
{
	printf("Usage : sudo %s host [-f first_ttl]\n	[-m max_ttl] [-q nqueries] [-w timeout] [-s paq_size]\n", s);
	exit(-1);
}

int	ft_cmp(char *s1, char *s2)
{
	int	i;
	i = 0;
	while (s1[i] && s1[i] == s2[i])
		i++;
	return (s1[i] - s2[i]);
}

int	init_addr(char *av, struct in_addr *addr)
{
	if (inet_pton(AF_INET, av, addr) <= 0)
		return (-1);
	return (socket(AF_INET, SOCK_RAW, IPPROTO_ICMP));
}

void	ft_bzero(void *s, size_t n)
{
	size_t	i;

	i = 0;
	while (i < n)
	{
		((u_char *)s)[i] = 0;
		i++;
	}
}

void	ft_cpy(char *s1, char *s2, int n)
{
	int	i;

	i = 0;
	while (i < n)
	{
		s1[i] = s2[i];
		i++;
	}
}

unsigned short in_cksum(unsigned short *addr, int len)
{
	register int		sum = 0;
	u_short			answer = 0;
	register u_short	*w = addr;
	register int		nleft = len;

	while (nleft > 1)
	{
		sum += *w++;
		nleft -= 2;
	}
	if (nleft == 1)
	{
		*(u_char *)(&answer) = *(u_char *)w;
		sum += answer;
	}
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	answer = ~sum;
	return (answer);
}

int	init_sock(char *host, struct in_addr *addr, struct sockaddr *s_addr)
{
	struct addrinfo		hint;
	struct addrinfo		*ainfo;
	struct addrinfo		*tmp;

	int			sock;

	hint.ai_flags = 0;
	hint.ai_family = AF_INET;
	hint.ai_socktype = SOCK_RAW;
	hint.ai_protocol = IPPROTO_ICMP;
	hint.ai_addrlen = 0;
	hint.ai_addr = NULL;
	hint.ai_canonname = NULL;
	hint.ai_next = NULL;

	if (getaddrinfo(host, NULL, &hint, &ainfo))
		perror("addrinfo"), exit(1);
	sock = -1;
	while (ainfo)
	{
		if (sock < 0)
		{
			s_addr->sa_family = ainfo->ai_addr->sa_family;
			ft_cpy(s_addr->sa_data, ainfo->ai_addr->sa_data, 14);
			sock = init_addr(inet_ntoa(((struct sockaddr_in *)s_addr)->sin_addr), addr);
		}
		tmp = ainfo;
		ainfo = ainfo->ai_next;
		free(tmp);
	}
	return (sock);
}

void	init_structs(struct iphdr *ip, struct icmphdr *icmp, size_t ip_len, size_t icmp_len, struct in_addr addr)
{
	struct in_addr	localhost;

	inet_pton(AF_INET, LOCALHOST, &localhost);
	ip->ihl = 5;
	ip->version = 4;
	ip->tos = 0;
	ip->tot_len = ip_len + icmp_len + g_glob.paq_len;
	ip->id = getpid();
	ip->ttl = g_glob.f_ttl;
	ip->protocol = IPPROTO_ICMP;
	ip->saddr = localhost.s_addr;
	ip->daddr = addr.s_addr;

	icmp->type = ICMP_ECHO;
	icmp->code = 0;
	icmp->un.echo.id = getpid();
	icmp->un.echo.sequence = 1;

	ip->check = 0;
	icmp->checksum = 0;

	ip->check = in_cksum((unsigned short *)ip, ip_len);
	icmp->checksum = in_cksum((unsigned short *)icmp, icmp_len + PAQ_SIZE);
}

void	send_sock(int socket, struct sockaddr s_addr, struct iphdr *ip, \
		void *send_buff, void *recv_buff, struct in_addr *tmpaddr)
{
	fd_set		rw;
	struct timeval	tim;
	int		ret;
	socklen_t	flen;
	struct timeval	last = {0, 0};
	struct timeval	now = {0, 0};
	t_time		ms;
	char		*tmp;

	FD_ZERO(&rw);
	FD_SET(socket, &rw);
	tim.tv_sec = g_glob.timeout;
	tim.tv_usec = 0;
	flen = sizeof(s_addr);

	gettimeofday(&last, NULL);
	ret = sendto(socket, send_buff, ip->tot_len, 0, &s_addr, sizeof(s_addr));

	select(socket + 1, &rw, NULL, NULL, &tim);
	if (FD_ISSET(socket, &rw))
	{
		gettimeofday(&now, NULL);
		ret = recvfrom(socket, recv_buff, ip->tot_len, 0, &s_addr, &flen);
		if (ret > 0)
		{
			ms.ms = (now.tv_usec - last.tv_usec) / 1000;
			ms.fms = (now.tv_usec - last.tv_usec) - (ms.ms * 1000);
			if (tmpaddr->s_addr == ((struct iphdr *)recv_buff)->saddr)
			{
				printf(" %2ld.%03ld ms", ms.ms, ms.fms);
				return ;
			}
			tmpaddr->s_addr = ((struct iphdr *)recv_buff)->saddr;
			tmp = inet_ntoa(*tmpaddr);
			printf(" %s (%s) %2ld.%03ld ms", \
			tmp, tmp, ms.ms, ms.fms);
			return ;
		}
	}
	printf(" *");
}

int	cmp(char *s1, char *s2)
{
	int	i;

	i = 0;
	while (s1[i] && s1[i] == s2[i])
		i++;
	return (s1[i] - s2[i]);
}

int	ft_atoi(char *s)
{
	int	nb;
	int	i;

	nb = 0;
	i = 0;
	if (!s)
		return (-1);
	while (s[i])
	{
		if (s[i] >= '9' || s[i] <= '0')
			return (-1);
		nb *= 10;
		nb += s[i] - '0';
		i++;
	}
	return (nb);
}

char	*parse_arg(char **av)
{
	char	*tmp;
	int	i;

	tmp = NULL;
	i = 1;
	while (av[i])
	{
		if (!cmp(av[i], "-f"))
		{
			i++;
			g_glob.f_ttl = ft_atoi(av[i]);
			if (g_glob.f_ttl <= 0)
				return (NULL);
		}
		else if (!cmp(av[i], "-q"))
		{
			i++;
			g_glob.queries = ft_atoi(av[i]);
			if (g_glob.queries <= 0)
				return (NULL);
		}
		else if (!cmp(av[i], "-w"))
		{
			i++;
			g_glob.timeout = ft_atoi(av[i]);
			if (g_glob.timeout <= 0)
				return (NULL);
		}
		else if (!cmp(av[i], "-m"))
		{
			i++;
			g_glob.mttl = ft_atoi(av[i]);
			if (g_glob.mttl <= 0)
				return (NULL);
		}
		else if (!cmp(av[i], "-s"))
		{
			i++;
			g_glob.paq_len = ft_atoi(av[i]);
			if (g_glob.paq_len <= 0)
				return (NULL);
		}
		else if (!tmp)
			tmp = av[i];
		else
			return (NULL);
		i++;
	}
	return (tmp);
}

int	main(int ac, char **av)
{
	int		socket;
	struct in_addr	addr;
	struct sockaddr	s_addr;
	char		*tmp;

	int		optval;

	size_t		ip_len;
	size_t		icmp_len;

	u_char		*send_buff;
	u_char		*recv_buff;

	struct iphdr	*ip;
	struct icmphdr	*icmp;

	if (ac < 2 || getuid() != 0)
		help(av[0]);
	tmp = parse_arg(av);
	if (!tmp)
		help(av[0]);
	socket = init_sock(tmp, &addr, &s_addr);
	if (socket < 0)
		perror("socket"), exit(2);
	optval = 1;
	if (setsockopt(socket, IPPROTO_IP, IP_HDRINCL, &optval, sizeof(int)))
		perror("setsockopt"), exit(3);

	ip_len = sizeof(struct iphdr);
	icmp_len = sizeof(struct icmphdr);
	send_buff = malloc(ip_len + icmp_len + PAQ_SIZE);
	recv_buff = malloc(ip_len + icmp_len + PAQ_SIZE);
	if (!send_buff || !recv_buff)
		exit(5);
	ft_bzero(send_buff, ip_len + icmp_len + PAQ_SIZE);
	ft_bzero(recv_buff, ip_len + icmp_len + PAQ_SIZE);
	ip = (struct iphdr *)send_buff;
	icmp = (struct icmphdr *)(send_buff + ip_len);
	init_structs(ip, icmp, ip_len, icmp_len, addr);

	struct in_addr	tmpaddr;
	ft_bzero(&tmpaddr, sizeof(tmpaddr));
	addr.s_addr = ip->daddr;

	printf("traceroute to %s (%s), %d hops max, %ld byte packets\n", \
			inet_ntoa(addr), inet_ntoa(addr), g_glob.mttl, ip->tot_len - ip_len);
	while (tmpaddr.s_addr != addr.s_addr && ip->ttl <= g_glob.mttl)
	{
		int	i;

		printf("%2d ", ip->ttl);
		i = 0;
		while (i < g_glob.queries)
		{
			send_sock(socket, s_addr, ip, send_buff, recv_buff, &tmpaddr);
			i++;
		}
		printf("\n");
		ip->ttl++;
		ip->check = 0;
		ip->check = in_cksum((unsigned short *)ip, ip_len);
	}
	free(recv_buff);
	free(send_buff);
	close(socket);
	return (0);
}
