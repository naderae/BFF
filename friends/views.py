from django.shortcuts import render, redirect
from friends.models import Friend
from django.contrib.auth.models import User
# Create your views here.


def change_friends(request, operation, pk):
    friend = User.objects.get(pk=pk)
    print (friend)
    if operation == 'add':
        friendship_one, created = Friend.objects.get_or_create(owner = request.user)
        friendship_one.users.add(friend)
        friendship_two, created = Friend.objects.get_or_create(owner = friend)
        friendship_two.users.add(request.user)

    elif operation == 'lose':
        friendship_one = Friend.objects.get(owner=request.user)
        friendship_one.users.remove(friend)
        if not friendship_one.users.exists():
             friendship_one.delete()
        friendship_two = Friend.objects.get(owner = friend)
        friendship_two.users.remove(request.user)
        if not friendship_two.users.exists():
             friendship_two.delete()

    return redirect('accounts:profile', pk)
